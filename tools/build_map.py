#!/usr/bin/env python3
"""Builds firmware/main/map.bin: coastlines, borders, airports and runways for Europe.

Sources (public domain): Natural Earth 1:10m, OurAirports.
Binary layout: see firmware/main/mapdata.h.
Points are (lat, lon) tuples in degrees everywhere in this module.
"""
import csv
import json
import re
import struct
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CACHE = Path(__file__).resolve().parent / ".cache"
OUT = ROOT / "firmware" / "main" / "map.bin"

# (lat_min, lon_min, lat_max, lon_max)
EUROPE = (34.0, -25.0, 72.0, 45.0)
TOLERANCE_DEG = 0.002  # about 200 m

COAST, BORDER = 0, 1
MEDIUM, LARGE = 0, 1

NE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/"
OA = "https://davidmegginson.github.io/ourairports-data/"
SOURCES = {
    "coast.geojson": NE + "ne_10m_coastline.geojson",
    "borders.geojson": NE + "ne_10m_admin_0_boundary_lines_land.geojson",
    "airports.csv": OA + "airports.csv",
    "runways.csv": OA + "runways.csv",
}


def clip(points, bbox):
    """Splits a polyline into the runs of consecutive points inside bbox.

    bbox is (lat_min, lon_min, lat_max, lon_max) in degrees, like EUROPE.
    Returns a list of runs (lists of points). Points on the bbox edge are
    inside. Runs of fewer than 2 points are dropped.
    """
    # ponytail: the segment crossing the bbox edge is dropped, not cut at the
    # edge. Invisible at the Europe bbox border; cut properly if the bbox shrinks.
    lat_min, lon_min, lat_max, lon_max = bbox
    runs, run = [], []
    for lat, lon in points:
        if lat_min <= lat <= lat_max and lon_min <= lon <= lon_max:
            run.append((lat, lon))
        else:
            if len(run) >= 2:
                runs.append(run)
            run = []
    if len(run) >= 2:
        runs.append(run)
    return runs


def simplify(points, tolerance):
    """Douglas-Peucker simplification, tolerance in degrees (planar distance).

    Always keeps the first and last point and the original order. Every
    dropped point lies within tolerance of the simplified line. Inputs of
    fewer than 3 points are returned unchanged (as a list).
    """
    points = list(points)
    if len(points) < 3:
        return points
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        a, b = stack.pop()
        (ay, ax), (by, bx) = points[a], points[b]
        dx, dy = bx - ax, by - ay
        norm = (dx * dx + dy * dy) ** 0.5
        worst, index = 0.0, -1
        for i in range(a + 1, b):
            py, px = points[i]
            if norm == 0:
                d = ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
            else:
                d = abs(dx * (ay - py) - dy * (ax - px)) / norm
            if d > worst:
                worst, index = d, i
        if worst > tolerance:
            keep[index] = True
            stack += [(a, index), (index, b)]
    return [p for p, k in zip(points, keep) if k]


def airports(rows, bbox):
    """Selects airports from OurAirports airports.csv rows (dicts).

    Keeps rows of type "medium_airport" or "large_airport" inside bbox whose
    code is exactly 4 uppercase letters/digits. The code is "icao_code" when
    present and non-empty, else "ident".
    bbox is (lat_min, lon_min, lat_max, lon_max) in degrees, like EUROPE.
    Returns a list of (icao, lat, lon, size) with size MEDIUM or LARGE.
    """
    lat_min, lon_min, lat_max, lon_max = bbox
    sizes = {"medium_airport": MEDIUM, "large_airport": LARGE}
    out = []
    for row in rows:
        code = row.get("icao_code") or row.get("ident") or ""
        if row.get("type") not in sizes or not re.fullmatch(r"[A-Z0-9]{4}", code):
            continue
        try:
            lat, lon = float(row["latitude_deg"]), float(row["longitude_deg"])
        except (KeyError, ValueError):
            continue
        if lat_min <= lat <= lat_max and lon_min <= lon <= lon_max:
            out.append((code, lat, lon, sizes[row["type"]]))
    return out


def runways(rows, idents):
    """Selects runways from OurAirports runways.csv rows (dicts).

    Keeps rows whose "airport_ident" is in idents, that are not closed
    ("closed" != "1") and have both ends' coordinates.
    Returns a list of (lat1, lon1, lat2, lon2): low end then high end.
    """
    out = []
    for row in rows:
        if row.get("airport_ident") not in idents or row.get("closed") == "1":
            continue
        try:
            out.append(tuple(float(row[k]) for k in (
                "le_latitude_deg", "le_longitude_deg", "he_latitude_deg", "he_longitude_deg")))
        except (KeyError, ValueError):
            continue
    return out


def _micro(deg):
    return round(deg * 1_000_000)


def pack(lines, airport_list, runway_list):
    """Serialises to the map.bin layout documented in firmware/main/mapdata.h.

    lines: list of (kind, points) with kind COAST or BORDER.
    airport_list: output of airports(). runway_list: output of runways().
    Returns bytes.
    """
    n_points = sum(len(points) for _, points in lines)
    out = [struct.pack("<4s4I", b"TSM1", len(lines), n_points, len(airport_list), len(runway_list))]
    first = 0
    for kind, points in lines:
        lats = [_micro(lat) for lat, _ in points]
        lons = [_micro(lon) for _, lon in points]
        out.append(struct.pack("<4i2IB3x", min(lats), min(lons), max(lats), max(lons),
                               first, len(points), kind))
        first += len(points)
    for _, points in lines:
        for lat, lon in points:
            out.append(struct.pack("<2i", _micro(lat), _micro(lon)))
    for icao, lat, lon, size in airport_list:
        out.append(struct.pack("<4s2iB3x", icao.encode("ascii"), _micro(lat), _micro(lon), size))
    for runway in runway_list:
        out.append(struct.pack("<4i", *(_micro(v) for v in runway)))
    return b"".join(out)


def geojson_lines(geojson):
    """Yields every polyline of a GeoJSON FeatureCollection as (lat, lon) points.

    Handles LineString and MultiLineString geometries (GeoJSON stores lon, lat).
    """
    for feature in geojson["features"]:
        geometry = feature["geometry"]
        parts = geometry["coordinates"]
        if geometry["type"] == "LineString":
            parts = [parts]
        elif geometry["type"] != "MultiLineString":
            continue
        for part in parts:
            yield [(lat, lon) for lon, lat, *_ in part]


def _fetch(name):
    path = CACHE / name
    if not path.exists():
        CACHE.mkdir(exist_ok=True)
        print("downloading", SOURCES[name])
        urllib.request.urlretrieve(SOURCES[name], path)
    return path


def main():
    lines = []
    for kind, name in ((COAST, "coast.geojson"), (BORDER, "borders.geojson")):
        for line in geojson_lines(json.loads(_fetch(name).read_text())):
            for run in clip(line, EUROPE):
                lines.append((kind, simplify(run, TOLERANCE_DEG)))
    with open(_fetch("airports.csv"), newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
        airport_list = airports(rows, EUROPE)
        kept = {(r.get("icao_code") or r["ident"]) for r in rows} & {a[0] for a in airport_list}
        # runways.csv refers to airports by "ident", which can differ from icao_code
        idents = {r["ident"] for r in rows if (r.get("icao_code") or r["ident"]) in kept}
    with open(_fetch("runways.csv"), newline="", encoding="utf-8") as f:
        runway_list = runways(csv.DictReader(f), idents)
    blob = pack(lines, airport_list, runway_list)
    OUT.write_bytes(blob)
    print(f"{OUT.relative_to(ROOT)}: {len(blob) / 1e6:.2f} MB, {len(lines)} lines, "
          f"{sum(len(p) for _, p in lines)} points, {len(airport_list)} airports, "
          f"{len(runway_list)} runways")


if __name__ == "__main__":
    sys.exit(main())

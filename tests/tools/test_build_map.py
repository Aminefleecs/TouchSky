import math
import random
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import build_map as bm  # noqa: E402

BOX = (0.0, 0.0, 10.0, 10.0)  # square, so lat/lon order of the bbox does not matter
ABOX = (40.0, -5.0, 50.0, 10.0)  # asymmetric: (lat_min, lon_min, lat_max, lon_max)


class ClipTest(unittest.TestCase):
    def test_all_inside(self):
        pts = [(1, 1), (2, 2), (3, 5)]
        self.assertEqual([list(r) for r in bm.clip(pts, BOX)], [pts])

    def test_all_outside(self):
        self.assertEqual(bm.clip([(20, 20), (21, 21), (22, 22)], BOX), [])

    def test_leave_and_reenter(self):
        pts = [(1, 1), (2, 2), (20, 20), (21, 21), (3, 3), (4, 4), (5, 5)]
        runs = [list(r) for r in bm.clip(pts, BOX)]
        self.assertEqual(runs, [[(1, 1), (2, 2)], [(3, 3), (4, 4), (5, 5)]])

    def test_short_runs_dropped(self):
        pts = [(1, 1), (20, 20), (2, 2), (3, 3), (30, 30), (4, 4)]
        runs = [list(r) for r in bm.clip(pts, BOX)]
        self.assertEqual(runs, [[(2, 2), (3, 3)]])

    def test_edge_points_inside(self):
        pts = [(0, 0), (10, 10), (0, 10)]
        self.assertEqual([list(r) for r in bm.clip(pts, BOX)], [pts])

    def test_just_outside_edge(self):
        self.assertEqual(bm.clip([(10.0001, 5), (5, 10.0001), (-0.0001, 5)], BOX), [])

    def test_asymmetric_bbox_lat_lon_order(self):
        pts = [(45.0, 0.0), (45.0, -4.0),  # inside
               (0.0, 45.0), (-4.0, 45.0),  # lat/lon swapped: outside
               (45.0, 1.0), (46.0, 2.0),  # inside
               (45.0, 20.0), (46.0, 20.0),  # outside by lon only
               (47.0, 3.0), (48.0, 4.0),  # inside
               (55.0, 0.0), (56.0, 1.0),  # outside by lat only
               (44.0, 5.0), (43.0, 6.0),  # inside
               (45.0, -6.0), (46.0, -7.0)]  # outside by negative lon
        runs = [list(r) for r in bm.clip(pts, ABOX)]
        self.assertEqual(runs, [pts[0:2], pts[4:6], pts[8:10], pts[12:14]])

    def test_asymmetric_bbox_edges_inside(self):
        pts = [(40.0, -5.0), (50.0, 10.0), (40.0, 10.0), (50.0, -5.0)]
        self.assertEqual([list(r) for r in bm.clip(pts, ABOX)], [pts])

    def test_europe_constant(self):
        paris, ny, dubai = (48.85, 2.35), (40.64, -73.78), (25.25, 55.36)
        pts = [paris, (48.9, 2.4), ny, (40.7, -73.8), dubai, (25.3, 55.4)]
        self.assertEqual([list(r) for r in bm.clip(pts, bm.EUROPE)], [pts[0:2]])


def seg_dist(p, a, b):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def poly_dist(p, line):
    return min(seg_dist(p, line[i], line[i + 1]) for i in range(len(line) - 1))


class SimplifyTest(unittest.TestCase):
    def test_collinear(self):
        pts = [(0, 0), (1, 1), (2, 2), (3, 3), (4, 4)]
        self.assertEqual(bm.simplify(pts, 0.01), [(0, 0), (4, 4)])

    def test_far_point_kept(self):
        pts = [(0, 0), (1, 1), (2, 0)]
        self.assertEqual(bm.simplify(pts, 0.5), pts)

    def test_near_point_dropped(self):
        pts = [(0, 0), (1, 0.1), (2, 0)]
        self.assertEqual(bm.simplify(pts, 0.5), [(0, 0), (2, 0)])

    def test_short_unchanged(self):
        self.assertEqual(bm.simplify([], 1), [])
        self.assertEqual(bm.simplify([(1, 2)], 1), [(1, 2)])
        out = bm.simplify([(1, 2), (3, 4)], 1)
        self.assertEqual(out, [(1, 2), (3, 4)])
        self.assertIsInstance(out, list)
        self.assertIsInstance(bm.simplify(((1, 2), (3, 4)), 1), list)

    def test_endpoints_and_order(self):
        rnd = random.Random(1)
        pts = [(i * 0.01, rnd.uniform(-1, 1)) for i in range(100)]
        for tol in (0.001, 0.1, 5):
            out = list(bm.simplify(pts, tol))
            self.assertEqual(out[0], pts[0])
            self.assertEqual(out[-1], pts[-1])
            it = iter(pts)
            self.assertTrue(all(any(p == q for q in it) for p in out))  # subsequence

    def test_dropped_points_within_tolerance(self):
        rnd = random.Random(42)
        walk = [(0.0, 0.0)]
        for _ in range(300):
            walk.append((walk[-1][0] + rnd.uniform(-0.3, 0.3), walk[-1][1] + rnd.uniform(-0.3, 0.3)))
        zigzag = [(i * 0.1, (i % 2) * 0.4) for i in range(50)]
        for pts in (walk, zigzag):
            for tol in (0.02, 0.1, 0.5):
                out = list(bm.simplify(pts, tol))
                self.assertLessEqual(len(out), len(pts))
                for p in pts:
                    self.assertLessEqual(poly_dist(p, out), tol + 1e-9, (tol, p))


def apt(**kw):
    row = {'type': 'large_airport', 'ident': 'LFPG', 'icao_code': 'LFPG',
           'latitude_deg': '5.0', 'longitude_deg': '5.0'}
    row.update(kw)
    return row


class AirportsTest(unittest.TestCase):
    def test_type_filter(self):
        rows = [apt(), apt(type='small_airport', ident='AAAA', icao_code='AAAA'),
                apt(type='heliport', ident='BBBB', icao_code='BBBB'),
                apt(type='medium_airport', ident='CCCC', icao_code='CCCC')]
        got = bm.airports(rows, BOX)
        self.assertEqual(sorted(g[0] for g in got), ['CCCC', 'LFPG'])

    def test_tuple_and_size(self):
        got = bm.airports([apt(latitude_deg='1.5', longitude_deg='2.5'),
                           apt(type='medium_airport', ident='EGLL', icao_code='EGLL')], BOX)
        self.assertEqual(len(got), 2)
        a, b = got
        self.assertEqual((a[0], a[3]), ('LFPG', bm.LARGE))
        self.assertAlmostEqual(a[1], 1.5)
        self.assertAlmostEqual(a[2], 2.5)
        self.assertEqual((b[0], b[3]), ('EGLL', bm.MEDIUM))

    def test_code_prefers_icao_code(self):
        got = bm.airports([apt(ident='LF99', icao_code='LFPG')], BOX)
        self.assertEqual([g[0] for g in got], ['LFPG'])

    def test_code_falls_back_to_ident(self):
        r1 = apt(ident='KJFK', icao_code='')
        r2 = apt(ident='EGLL')
        del r2['icao_code']
        got = bm.airports([r1, r2], BOX)
        self.assertEqual([g[0] for g in got], ['KJFK', 'EGLL'])

    def test_code_must_be_four_chars(self):
        rows = [apt(ident='LFP', icao_code='LFP'), apt(ident='LFPGX', icao_code='LFPGX'),
                apt(ident='LF-G', icao_code='LF-G'), apt(ident='lfpg', icao_code='lfpg'),
                apt(ident='LF9X', icao_code='LF9X')]
        self.assertEqual([g[0] for g in bm.airports(rows, BOX)], ['LF9X'])

    def test_bbox_filter(self):
        rows = [apt(ident='AAAA', icao_code='AAAA', latitude_deg='11', longitude_deg='5'),
                apt(ident='BBBB', icao_code='BBBB', latitude_deg='5', longitude_deg='11'),
                apt(ident='CCCC', icao_code='CCCC', latitude_deg='-1', longitude_deg='5'),
                apt(ident='DDDD', icao_code='DDDD', latitude_deg='5', longitude_deg='-1'),
                apt(ident='EEEE', icao_code='EEEE', latitude_deg='9', longitude_deg='1')]
        self.assertEqual([g[0] for g in bm.airports(rows, BOX)], ['EEEE'])

    def test_unparsable_coordinates_skipped(self):
        rows = [apt(ident='AAAA', icao_code='AAAA', latitude_deg=''),
                apt(ident='BBBB', icao_code='BBBB', longitude_deg='abc'),
                apt(ident='CCCC', icao_code='CCCC')]
        self.assertEqual([g[0] for g in bm.airports(rows, BOX)], ['CCCC'])

    def test_asymmetric_bbox(self):
        rows = [apt(ident='INSD', icao_code='INSD', latitude_deg='45', longitude_deg='0'),
                apt(ident='NEGL', icao_code='NEGL', latitude_deg='45', longitude_deg='-4'),
                apt(ident='SWAP', icao_code='SWAP', latitude_deg='0', longitude_deg='45'),
                apt(ident='OLON', icao_code='OLON', latitude_deg='45', longitude_deg='20'),
                apt(ident='OLAT', icao_code='OLAT', latitude_deg='55', longitude_deg='0'),
                apt(ident='NLON', icao_code='NLON', latitude_deg='45', longitude_deg='-6'),
                apt(ident='EDGE', icao_code='EDGE', latitude_deg='40', longitude_deg='10')]
        got = bm.airports(rows, ABOX)
        self.assertEqual([g[0] for g in got], ['INSD', 'NEGL', 'EDGE'])

    def test_asymmetric_tuple_order(self):
        got = bm.airports([apt(latitude_deg='45.5', longitude_deg='-3.5')], ABOX)
        self.assertEqual(len(got), 1)
        self.assertEqual(got[0][0], 'LFPG')
        self.assertAlmostEqual(got[0][1], 45.5)
        self.assertAlmostEqual(got[0][2], -3.5)
        self.assertEqual(got[0][3], bm.LARGE)

    def test_europe_constant(self):
        rows = [apt(ident='LFPG', icao_code='LFPG', latitude_deg='48.85', longitude_deg='2.35'),
                apt(ident='KJFK', icao_code='KJFK', latitude_deg='40.64', longitude_deg='-73.78'),
                apt(ident='OMDB', icao_code='OMDB', latitude_deg='25.25', longitude_deg='55.36')]
        self.assertEqual([g[0] for g in bm.airports(rows, bm.EUROPE)], ['LFPG'])


def rwy(**kw):
    row = {'airport_ident': 'LFPG', 'closed': '0',
           'le_latitude_deg': '1.0', 'le_longitude_deg': '2.0',
           'he_latitude_deg': '3.0', 'he_longitude_deg': '4.0'}
    row.update(kw)
    return row


class RunwaysTest(unittest.TestCase):
    def test_order_low_then_high(self):
        got = bm.runways([rwy()], {'LFPG'})
        self.assertEqual([tuple(g) for g in got], [(1.0, 2.0, 3.0, 4.0)])

    def test_ident_filter(self):
        rows = [rwy(), rwy(airport_ident='EGLL'), rwy(airport_ident='ZZZZ')]
        self.assertEqual(len(bm.runways(rows, {'LFPG', 'EGLL'})), 2)
        self.assertEqual(len(bm.runways(rows, {'EGLL'})), 1)
        self.assertEqual(bm.runways(rows, set()), [])

    def test_closed_skipped(self):
        rows = [rwy(closed='1'), rwy(closed='0', le_latitude_deg='7.0'), rwy(closed='', le_latitude_deg='8.0')]
        got = bm.runways(rows, {'LFPG'})
        self.assertEqual([g[0] for g in got], [7.0, 8.0])

    def test_missing_coordinates_skipped(self):
        for k in ('le_latitude_deg', 'le_longitude_deg', 'he_latitude_deg', 'he_longitude_deg'):
            self.assertEqual(bm.runways([rwy(**{k: ''})], {'LFPG'}), [], k)
        self.assertEqual(len(bm.runways([rwy(), rwy(he_latitude_deg='')], {'LFPG'})), 1)


class PackTest(unittest.TestCase):
    def setUp(self):
        self.lines = [
            (bm.COAST, [(48.8566, 2.3522), (-1.2345678, 3.0), (10.0, -20.5)]),
            (bm.BORDER, [(0.0, 0.0), (1.0, -1.0)]),
            (bm.COAST, [(5.5, 6.5), (4.5, 7.5), (6.0, 5.0), (5.0, 5.0)]),
        ]
        self.airports = [('LFPG', 49.0097, 2.5479, bm.LARGE), ('EGLL', 51.47, -0.4543, bm.MEDIUM)]
        self.runways = [(49.01, 2.54, 49.02, 2.56), (51.0, -1.0, 51.5, -0.5)]
        self.data = bm.pack(self.lines, self.airports, self.runways)

    def test_header_and_length(self):
        d = self.data
        self.assertIsInstance(d, bytes)
        self.assertEqual(d[:4], b'TSM1')
        self.assertEqual(struct.unpack_from('<4I', d, 4), (3, 9, 2, 2))
        self.assertEqual(len(d), 20 + 3 * 28 + 9 * 8 + 2 * 16 + 2 * 16)

    def test_lines(self):
        d = self.data
        first = 0
        for i, (kind, pts) in enumerate(self.lines):
            mnl, mnn, mxl, mxn, fp, n, k = struct.unpack_from('<4i2IB', d, 20 + 28 * i)
            lats = [round(p[0] * 1e6) for p in pts]
            lons = [round(p[1] * 1e6) for p in pts]
            self.assertEqual((mnl, mnn, mxl, mxn), (min(lats), min(lons), max(lats), max(lons)))
            self.assertEqual((fp, n, k), (first, len(pts), kind))
            first += len(pts)

    def test_points_rounding(self):
        base = 20 + 3 * 28
        pts = [p for _, ps in self.lines for p in ps]
        for i, (lat, lon) in enumerate(pts):
            got = struct.unpack_from('<2i', self.data, base + 8 * i)
            self.assertEqual(got, (round(lat * 1e6), round(lon * 1e6)))
        self.assertEqual(struct.unpack_from('<2i', self.data, base), (48856600, 2352200))
        self.assertEqual(struct.unpack_from('<2i', self.data, base + 8)[0], -1234568)  # rounded, not truncated

    def test_airports(self):
        base = 20 + 3 * 28 + 9 * 8
        for i, (code, lat, lon, size) in enumerate(self.airports):
            c, la, lo, s = struct.unpack_from('<4s2iB', self.data, base + 16 * i)
            self.assertEqual((c, la, lo, s), (code.encode(), round(lat * 1e6), round(lon * 1e6), size))

    def test_runways(self):
        base = 20 + 3 * 28 + 9 * 8 + 2 * 16
        for i, r in enumerate(self.runways):
            self.assertEqual(struct.unpack_from('<4i', self.data, base + 16 * i),
                             tuple(round(v * 1e6) for v in r))

    def test_empty(self):
        d = bm.pack([], [], [])
        self.assertEqual(d, b'TSM1' + struct.pack('<4I', 0, 0, 0, 0))


class GeojsonLinesTest(unittest.TestCase):
    def test_linestring_and_multilinestring(self):
        gj = {'type': 'FeatureCollection', 'features': [
            {'type': 'Feature', 'geometry': {'type': 'LineString', 'coordinates': [[2.0, 48.0], [3.0, 49.0]]}},
            {'type': 'Feature', 'geometry': {'type': 'MultiLineString', 'coordinates': [
                [[10.0, 40.0], [11.0, 41.0], [12.0, 42.0]], [[-5.0, 30.0], [-4.0, 31.0]]]}},
        ]}
        got = [[tuple(p) for p in line] for line in bm.geojson_lines(gj)]
        self.assertEqual(got, [
            [(48.0, 2.0), (49.0, 3.0)],
            [(40.0, 10.0), (41.0, 11.0), (42.0, 12.0)],
            [(30.0, -5.0), (31.0, -4.0)],
        ])

    def test_other_geometries_ignored(self):
        gj = {'type': 'FeatureCollection', 'features': [
            {'type': 'Feature', 'geometry': {'type': 'Point', 'coordinates': [1.0, 2.0]}},
            {'type': 'Feature', 'geometry': {'type': 'Polygon', 'coordinates': [[[0, 0], [1, 0], [1, 1], [0, 0]]]}},
            {'type': 'Feature', 'geometry': {'type': 'MultiPoint', 'coordinates': [[1.0, 2.0]]}},
            {'type': 'Feature', 'geometry': {'type': 'LineString', 'coordinates': [[7.0, 8.0], [9.0, 10.0]]}},
        ]}
        got = [[tuple(p) for p in line] for line in bm.geojson_lines(gj)]
        self.assertEqual(got, [[(8.0, 7.0), (10.0, 9.0)]])

    def test_empty(self):
        self.assertEqual(list(bm.geojson_lines({'type': 'FeatureCollection', 'features': []})), [])


if __name__ == '__main__':
    unittest.main()

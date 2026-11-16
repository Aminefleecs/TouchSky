// adsb.lol response parsing and aircraft formatting.
// Pure C (cJSON only), no ESP-IDF dependency (host-testable).
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define ADSB_MAX_AIRCRAFT 512

typedef struct {
    char hex[7];      // ICAO 24-bit address as sent by the API, e.g. "3848bd"
    char callsign[9]; // "flight" field with surrounding spaces removed, "" if absent
    double lat, lon;  // degrees
    bool has_alt;     // false when "alt_baro" is absent
    bool on_ground;   // true when "alt_baro" is the string "ground"
    int alt_ft;       // barometric altitude in feet, 0 when !has_alt or on_ground
    bool has_track;   // false when "track" is absent
    float track_deg;  // true track, 0..360, 0 = north, clockwise
    float gs_kt;      // ground speed in knots, 0 if absent
    float dist_nm;    // distance from the centre given to adsb_parse()
} adsb_aircraft_t;

// Writes the request URL for all aircraft within range_nm of (lat, lon):
// "https://api.adsb.lol/v2/point/<lat>/<lon>/<range_nm>", lat/lon with 4 decimals.
// Returns false if buf is too small.
bool adsb_build_url(char *buf, size_t size, double lat, double lon, int range_nm);

// Parses an adsb.lol /v2/point response (object with an "ac" array).
// Aircraft without "lat"/"lon" are skipped. dist_nm is computed from
// (center_lat, center_lon). Output is sorted by increasing dist_nm and
// truncated to max entries (the closest ones are kept).
// Returns the number of aircraft written, or -1 if json is not valid JSON
// or has no "ac" array.
int adsb_parse(const char *json, double center_lat, double center_lon,
               adsb_aircraft_t *out, int max);

// Name shown for an aircraft: its callsign, or its hex address when the
// callsign is empty. The returned pointer lives as long as *ac.
const char *adsb_name(const adsb_aircraft_t *ac);

// Altitude text: "GND" on the ground, "---" when unknown, "FL350" style
// (hundreds of feet, rounded to nearest, 3 digits) at or above 10000 ft,
// "4500ft" style below. Always NUL-terminates if size > 0.
void adsb_format_alt(const adsb_aircraft_t *ac, char *buf, size_t size);

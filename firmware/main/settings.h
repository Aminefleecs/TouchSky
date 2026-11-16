// User settings: validation (pure C, host-testable) and NVS storage
// (settings_store.c, device only).
#pragma once

#include <stdbool.h>

typedef struct {
    char ssid[33];
    char pass[65];
    double lat, lon; // radar centre, degrees
    int range_nm;    // one of 25, 50, 100, 250
} settings_t;

// Parses a decimal coordinate typed by the user ("48.85", "-2.5", " 7 ").
// Surrounding spaces are allowed; empty text, trailing junk, NaN/inf and
// values outside [min, max] are rejected. *out is written only on success.
bool settings_parse_coord(const char *text, double min, double max, double *out);

// True for the ranges offered in the UI: 25, 50, 100, 250.
bool settings_range_valid(int range_nm);

// True when the radar can start: non-empty ssid, lat in [-90, 90],
// lon in [-180, 180], valid range. An empty password is fine (open network).
bool settings_complete(const settings_t *s);

// NVS storage. settings_load() returns false and zeroes *s when nothing
// (or something incomplete) is stored.
bool settings_load(settings_t *s);
void settings_save(const settings_t *s);

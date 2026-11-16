#include "settings.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

bool settings_parse_coord(const char *text, double min, double max, double *out)
{
    // strtod alone would accept "nan", "inf" and hex floats: allow only
    // [space] [sign] digits [. digits] [space].
    const char *p = text;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    const char *start = p;
    if (*p == '-' || *p == '+') {
        p++;
    }
    int digits = 0, dots = 0;
    for (; isdigit((unsigned char)*p) || *p == '.'; p++) {
        if (*p == '.') {
            dots++;
        } else {
            digits++;
        }
    }
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '\0' || digits == 0 || dots > 1) {
        return false;
    }
    double v = strtod(start, NULL);
    if (!isfinite(v) || v < min || v > max) {
        return false;
    }
    *out = v;
    return true;
}

bool settings_range_valid(int range_nm)
{
    return range_nm == 25 || range_nm == 50 || range_nm == 100 || range_nm == 250;
}

bool settings_complete(const settings_t *s)
{
    return s->ssid[0] != '\0' &&
           s->lat >= -90 && s->lat <= 90 && s->lon >= -180 && s->lon <= 180 &&
           settings_range_valid(s->range_nm);
}

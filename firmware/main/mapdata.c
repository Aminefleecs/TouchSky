#include "mapdata.h"

#include <ctype.h>
#include <string.h>

enum { HEADER_SIZE = 20, LINE_SIZE = 28, POINT_SIZE = 8, AIRPORT_SIZE = 16, RUNWAY_SIZE = 16 };

// The target and the host are both little-endian, so fields are copied as is;
// memcpy keeps unaligned blobs safe.
static uint32_t u32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static int32_t i32(const uint8_t *p)
{
    return (int32_t)u32(p);
}

bool mapdata_open(mapdata_t *map, const void *blob, size_t len)
{
    const uint8_t *p = blob;
    if (len < HEADER_SIZE || memcmp(p, "TSM1", 4) != 0) {
        return false;
    }
    map->n_lines = u32(p + 4);
    map->n_points = u32(p + 8);
    map->n_airports = u32(p + 12);
    map->n_runways = u32(p + 16);
    uint64_t expected = HEADER_SIZE + (uint64_t)map->n_lines * LINE_SIZE +
                        (uint64_t)map->n_points * POINT_SIZE +
                        (uint64_t)map->n_airports * AIRPORT_SIZE +
                        (uint64_t)map->n_runways * RUNWAY_SIZE;
    if (expected != len) {
        return false;
    }
    map->lines = p + HEADER_SIZE;
    map->points = map->lines + (size_t)map->n_lines * LINE_SIZE;
    map->airports = map->points + (size_t)map->n_points * POINT_SIZE;
    map->runways = map->airports + (size_t)map->n_airports * AIRPORT_SIZE;
    return true;
}

map_line_t mapdata_line(const mapdata_t *map, uint32_t i)
{
    const uint8_t *p = map->lines + (size_t)i * LINE_SIZE;
    return (map_line_t){
        .min_lat = i32(p), .min_lon = i32(p + 4), .max_lat = i32(p + 8), .max_lon = i32(p + 12),
        .first_point = u32(p + 16), .n_points = u32(p + 20), .kind = p[24],
    };
}

map_point_t mapdata_point(const mapdata_t *map, uint32_t i)
{
    const uint8_t *p = map->points + (size_t)i * POINT_SIZE;
    return (map_point_t){.lat = i32(p), .lon = i32(p + 4)};
}

map_airport_t mapdata_airport(const mapdata_t *map, uint32_t i)
{
    const uint8_t *p = map->airports + (size_t)i * AIRPORT_SIZE;
    map_airport_t a = {.lat = i32(p + 4), .lon = i32(p + 8), .size = p[12]};
    memcpy(a.icao, p, 4);
    return a;
}

map_runway_t mapdata_runway(const mapdata_t *map, uint32_t i)
{
    const uint8_t *p = map->runways + (size_t)i * RUNWAY_SIZE;
    return (map_runway_t){.lat1 = i32(p), .lon1 = i32(p + 4), .lat2 = i32(p + 8), .lon2 = i32(p + 12)};
}

int mapdata_find_airport(const mapdata_t *map, const char *icao)
{
    if (strlen(icao) != 4) {
        return -1;
    }
    char code[4];
    for (int i = 0; i < 4; i++) {
        code[i] = (char)toupper((unsigned char)icao[i]);
    }
    // ponytail: linear scan over a few thousand airports, only on user input.
    for (uint32_t i = 0; i < map->n_airports; i++) {
        if (memcmp(map->airports + (size_t)i * AIRPORT_SIZE, code, 4) == 0) {
            return (int)i;
        }
    }
    return -1;
}

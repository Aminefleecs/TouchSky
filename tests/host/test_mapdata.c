#include <string.h>
#include "unity.h"
#include "mapdata.h"

void setUp(void) {}
void tearDown(void) {}

// Blob built byte by byte, little-endian, per the layout in mapdata.h.
typedef struct { uint8_t b[1024]; size_t n; } buf_t;

static void put8(buf_t *w, uint8_t v) { w->b[w->n++] = v; }
static void putu32(buf_t *w, uint32_t v)
{
    for (int i = 0; i < 4; i++) put8(w, (uint8_t)(v >> (8 * i)));
}
static void puti32(buf_t *w, int32_t v) { putu32(w, (uint32_t)v); }
static void putbytes(buf_t *w, const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) put8(w, (uint8_t)s[i]);
}

static void header(buf_t *w, const char *magic, uint32_t l, uint32_t p, uint32_t a, uint32_t r)
{
    w->n = 0;
    putbytes(w, magic, 4);
    putu32(w, l); putu32(w, p); putu32(w, a); putu32(w, r);
}

static void line(buf_t *w, int32_t a, int32_t b, int32_t c, int32_t d, uint32_t first, uint32_t np, uint8_t kind)
{
    puti32(w, a); puti32(w, b); puti32(w, c); puti32(w, d);
    putu32(w, first); putu32(w, np);
    put8(w, kind); put8(w, 0); put8(w, 0); put8(w, 0);
}

static void airport(buf_t *w, const char *icao, int32_t lat, int32_t lon, uint8_t size)
{
    putbytes(w, icao, 4);
    puti32(w, lat); puti32(w, lon);
    put8(w, size); put8(w, 0); put8(w, 0); put8(w, 0);
}

// 2 lines, 5 points, 2 airports, 1 runway
static void sample(buf_t *w)
{
    header(w, "TSM1", 2, 5, 2, 1);
    line(w, -1500000, -2500000, 3000000, 4000000, 0, 3, MAP_LINE_COAST);
    line(w, 48000000, 2000000, 49000000, 3000000, 3, 2, MAP_LINE_BORDER);
    puti32(w, -1500000); puti32(w, -2500000);
    puti32(w, 3000000);  puti32(w, 4000000);
    puti32(w, 0);        puti32(w, 0);
    puti32(w, 48000000); puti32(w, 2000000);
    puti32(w, 49000000); puti32(w, 3000000);
    airport(w, "LFPG", 49009700, 2547900, MAP_AIRPORT_LARGE);
    airport(w, "EGLL", 51470000, -454300, MAP_AIRPORT_MEDIUM);
    puti32(w, 49010000); puti32(w, 2540000); puti32(w, 49020000); puti32(w, 2560000);
}

static void check_sample(const mapdata_t *m)
{
    TEST_ASSERT_EQUAL_UINT32(2, m->n_lines);
    TEST_ASSERT_EQUAL_UINT32(5, m->n_points);
    TEST_ASSERT_EQUAL_UINT32(2, m->n_airports);
    TEST_ASSERT_EQUAL_UINT32(1, m->n_runways);

    map_line_t l0 = mapdata_line(m, 0), l1 = mapdata_line(m, 1);
    TEST_ASSERT_EQUAL_INT32(-1500000, l0.min_lat);
    TEST_ASSERT_EQUAL_INT32(-2500000, l0.min_lon);
    TEST_ASSERT_EQUAL_INT32(3000000, l0.max_lat);
    TEST_ASSERT_EQUAL_INT32(4000000, l0.max_lon);
    TEST_ASSERT_EQUAL_UINT32(0, l0.first_point);
    TEST_ASSERT_EQUAL_UINT32(3, l0.n_points);
    TEST_ASSERT_EQUAL_UINT8(MAP_LINE_COAST, l0.kind);
    TEST_ASSERT_EQUAL_INT32(48000000, l1.min_lat);
    TEST_ASSERT_EQUAL_INT32(2000000, l1.min_lon);
    TEST_ASSERT_EQUAL_INT32(49000000, l1.max_lat);
    TEST_ASSERT_EQUAL_INT32(3000000, l1.max_lon);
    TEST_ASSERT_EQUAL_UINT32(3, l1.first_point);
    TEST_ASSERT_EQUAL_UINT32(2, l1.n_points);
    TEST_ASSERT_EQUAL_UINT8(MAP_LINE_BORDER, l1.kind);

    static const int32_t pts[5][2] = {{-1500000, -2500000}, {3000000, 4000000}, {0, 0},
                                      {48000000, 2000000}, {49000000, 3000000}};
    for (uint32_t i = 0; i < 5; i++) {
        map_point_t p = mapdata_point(m, i);
        TEST_ASSERT_EQUAL_INT32(pts[i][0], p.lat);
        TEST_ASSERT_EQUAL_INT32(pts[i][1], p.lon);
    }

    map_airport_t a0 = mapdata_airport(m, 0), a1 = mapdata_airport(m, 1);
    TEST_ASSERT_EQUAL_STRING("LFPG", a0.icao);
    TEST_ASSERT_EQUAL_INT32(49009700, a0.lat);
    TEST_ASSERT_EQUAL_INT32(2547900, a0.lon);
    TEST_ASSERT_EQUAL_UINT8(MAP_AIRPORT_LARGE, a0.size);
    TEST_ASSERT_EQUAL_STRING("EGLL", a1.icao);
    TEST_ASSERT_EQUAL_INT32(51470000, a1.lat);
    TEST_ASSERT_EQUAL_INT32(-454300, a1.lon);
    TEST_ASSERT_EQUAL_UINT8(MAP_AIRPORT_MEDIUM, a1.size);

    map_runway_t r = mapdata_runway(m, 0);
    TEST_ASSERT_EQUAL_INT32(49010000, r.lat1);
    TEST_ASSERT_EQUAL_INT32(2540000, r.lon1);
    TEST_ASSERT_EQUAL_INT32(49020000, r.lat2);
    TEST_ASSERT_EQUAL_INT32(2560000, r.lon2);
}

void test_open_and_accessors(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    TEST_ASSERT_EQUAL_size_t(20 + 2 * 28 + 5 * 8 + 2 * 16 + 16, w.n);
    TEST_ASSERT_TRUE(mapdata_open(&m, w.b, w.n));
    check_sample(&m);
}

void test_open_bad_magic(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    w.b[3] = '2';
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n));
    sample(&w);
    w.b[0] = 'X';
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n));
}

void test_open_len_too_short(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n - 1));
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, 20));
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, 19));
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, 0));
}

void test_open_len_too_long(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    w.b[w.n] = 0; // blob has room, but the declared length is one byte too many
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n + 1));
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n + 16));
}

void test_open_huge_counts_do_not_wrap(void)
{
    buf_t w; mapdata_t m;
    header(&w, "TSM1", 0, 0x20000000u, 0, 0); // 0x20000000 * 8 wraps to 0 in 32 bits
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n));
    header(&w, "TSM1", 0x10000000u, 0, 0, 0); // * 16 wraps to 0
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n));
    header(&w, "TSM1", 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
    TEST_ASSERT_FALSE(mapdata_open(&m, w.b, w.n));
}

void test_open_empty_map(void)
{
    buf_t w; mapdata_t m;
    header(&w, "TSM1", 0, 0, 0, 0);
    TEST_ASSERT_EQUAL_size_t(20, w.n);
    TEST_ASSERT_TRUE(mapdata_open(&m, w.b, w.n));
    TEST_ASSERT_EQUAL_UINT32(0, m.n_lines);
    TEST_ASSERT_EQUAL_UINT32(0, m.n_points);
    TEST_ASSERT_EQUAL_UINT32(0, m.n_airports);
    TEST_ASSERT_EQUAL_UINT32(0, m.n_runways);
    TEST_ASSERT_EQUAL_INT(-1, mapdata_find_airport(&m, "LFPG"));
}

void test_unaligned_blob(void)
{
    buf_t w; mapdata_t m;
    static uint8_t copy[1100];
    sample(&w);
    for (int off = 1; off <= 3; off++) {
        memcpy(copy + off, w.b, w.n);
        TEST_ASSERT_TRUE(mapdata_open(&m, copy + off, w.n));
        check_sample(&m);
        TEST_ASSERT_EQUAL_INT(1, mapdata_find_airport(&m, "EGLL"));
    }
}

void test_find_airport(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    TEST_ASSERT_TRUE(mapdata_open(&m, w.b, w.n));
    TEST_ASSERT_EQUAL_INT(0, mapdata_find_airport(&m, "LFPG"));
    TEST_ASSERT_EQUAL_INT(1, mapdata_find_airport(&m, "EGLL"));
    TEST_ASSERT_EQUAL_INT(0, mapdata_find_airport(&m, "lfpg"));
    TEST_ASSERT_EQUAL_INT(1, mapdata_find_airport(&m, "egll"));
    TEST_ASSERT_EQUAL_INT(1, mapdata_find_airport(&m, "EgLl"));
    TEST_ASSERT_EQUAL_INT(-1, mapdata_find_airport(&m, "ZZZZ"));
    TEST_ASSERT_EQUAL_INT(-1, mapdata_find_airport(&m, "LFP"));
    TEST_ASSERT_EQUAL_INT(-1, mapdata_find_airport(&m, "LFPGX"));
    TEST_ASSERT_EQUAL_INT(-1, mapdata_find_airport(&m, ""));
}

void test_airport_icao_nul_terminated(void)
{
    buf_t w; mapdata_t m;
    sample(&w);
    TEST_ASSERT_TRUE(mapdata_open(&m, w.b, w.n));
    for (uint32_t i = 0; i < 2; i++) {
        map_airport_t a;
        memset(&a, 'X', sizeof a);
        a = mapdata_airport(&m, i);
        TEST_ASSERT_EQUAL_CHAR(0, a.icao[4]);
        TEST_ASSERT_EQUAL_size_t(4, strlen(a.icao));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_and_accessors);
    RUN_TEST(test_open_bad_magic);
    RUN_TEST(test_open_len_too_short);
    RUN_TEST(test_open_len_too_long);
    RUN_TEST(test_open_huge_counts_do_not_wrap);
    RUN_TEST(test_open_empty_map);
    RUN_TEST(test_unaligned_blob);
    RUN_TEST(test_find_airport);
    RUN_TEST(test_airport_icao_nul_terminated);
    return UNITY_END();
}

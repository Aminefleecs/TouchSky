#include <string.h>
#include "unity.h"
#include "settings.h"

void setUp(void) {}
void tearDown(void) {}

#define SENTINEL 123.456

static void ok(const char *t, double min, double max, double want)
{
    double v = SENTINEL;
    TEST_ASSERT_TRUE_MESSAGE(settings_parse_coord(t, min, max, &v), t);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, want, v);
}

static void bad(const char *t, double min, double max)
{
    double v = SENTINEL;
    TEST_ASSERT_FALSE_MESSAGE(settings_parse_coord(t, min, max, &v), t);
    TEST_ASSERT_EQUAL_DOUBLE(SENTINEL, v);
}

void test_coord_valid(void)
{
    ok("48.85", -90, 90, 48.85);
    ok("-2.5", -180, 180, -2.5);
    ok("7", -90, 90, 7);
    ok("0", -90, 90, 0);
    ok(" 7 ", -90, 90, 7);
    ok("  -12.25  ", -180, 180, -12.25);
}

void test_coord_boundaries(void)
{
    ok("-90", -90, 90, -90);
    ok("90", -90, 90, 90);
    ok("180.0", -180, 180, 180);
    ok("-180", -180, 180, -180);
    ok("5", 5, 10, 5);
    ok("10", 5, 10, 10);
}

void test_coord_rejects_empty(void)
{
    bad("", -90, 90);
    bad("   ", -90, 90);
}

void test_coord_rejects_junk(void)
{
    bad("abc", -90, 90);
    bad("12abc", -90, 90);
    bad("1.2.3", -90, 90);
    bad("1 2", -90, 90);
}

void test_coord_rejects_nan_inf(void)
{
    bad("nan", -90, 90);
    bad("NaN", -90, 90);
    bad("inf", -90, 90);
    bad("-inf", -90, 90);
    bad("infinity", -90, 90);
}

void test_coord_rejects_out_of_range(void)
{
    bad("90.0001", -90, 90);
    bad("-90.0001", -90, 90);
    bad("91", -90, 90);
    bad("181", -180, 180);
    bad("4.99", 5, 10);
    bad("10.01", 5, 10);
}

void test_range_valid(void)
{
    TEST_ASSERT_TRUE(settings_range_valid(25));
    TEST_ASSERT_TRUE(settings_range_valid(50));
    TEST_ASSERT_TRUE(settings_range_valid(100));
    TEST_ASSERT_TRUE(settings_range_valid(250));
    TEST_ASSERT_FALSE(settings_range_valid(0));
    TEST_ASSERT_FALSE(settings_range_valid(-25));
    TEST_ASSERT_FALSE(settings_range_valid(26));
    TEST_ASSERT_FALSE(settings_range_valid(500));
}

static settings_t good(void)
{
    settings_t s;
    memset(&s, 0, sizeof s);
    strcpy(s.ssid, "home");
    strcpy(s.pass, "secret");
    s.lat = 48.85;
    s.lon = 2.35;
    s.range_nm = 50;
    return s;
}

void test_complete_ok(void)
{
    settings_t s = good();
    TEST_ASSERT_TRUE(settings_complete(&s));
}

void test_complete_empty_ssid(void)
{
    settings_t s = good();
    s.ssid[0] = 0;
    TEST_ASSERT_FALSE(settings_complete(&s));
}

void test_complete_bad_lat(void)
{
    settings_t s = good();
    s.lat = 90.5;
    TEST_ASSERT_FALSE(settings_complete(&s));
    s.lat = -90.5;
    TEST_ASSERT_FALSE(settings_complete(&s));
}

void test_complete_bad_lon(void)
{
    settings_t s = good();
    s.lon = 180.5;
    TEST_ASSERT_FALSE(settings_complete(&s));
    s.lon = -180.5;
    TEST_ASSERT_FALSE(settings_complete(&s));
}

void test_complete_bad_range(void)
{
    settings_t s = good();
    s.range_nm = 30;
    TEST_ASSERT_FALSE(settings_complete(&s));
    s.range_nm = 0;
    TEST_ASSERT_FALSE(settings_complete(&s));
}

void test_complete_empty_pass_ok(void)
{
    settings_t s = good();
    s.pass[0] = 0;
    TEST_ASSERT_TRUE(settings_complete(&s));
}

void test_complete_extreme_coords_ok(void)
{
    settings_t s = good();
    s.lat = -90; s.lon = -180;
    TEST_ASSERT_TRUE(settings_complete(&s));
    s.lat = 90; s.lon = 180;
    TEST_ASSERT_TRUE(settings_complete(&s));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_coord_valid);
    RUN_TEST(test_coord_boundaries);
    RUN_TEST(test_coord_rejects_empty);
    RUN_TEST(test_coord_rejects_junk);
    RUN_TEST(test_coord_rejects_nan_inf);
    RUN_TEST(test_coord_rejects_out_of_range);
    RUN_TEST(test_range_valid);
    RUN_TEST(test_complete_ok);
    RUN_TEST(test_complete_empty_ssid);
    RUN_TEST(test_complete_bad_lat);
    RUN_TEST(test_complete_bad_lon);
    RUN_TEST(test_complete_bad_range);
    RUN_TEST(test_complete_empty_pass_ok);
    RUN_TEST(test_complete_extreme_coords_ok);
    return UNITY_END();
}

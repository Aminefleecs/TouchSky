// Host simulator: runs the firmware's radar and settings screens headless
// and saves each as a BMP. Aircraft come from a captured adsb.lol response.
// Usage: touchsky_sim <map.bin> <adsb.json> <lat> <lon> <range_nm> <out_dir>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adsb.h"
#include "lvgl.h"
#include "mapdata.h"
#include "net.h"
#include "radar_ui.h"
#include "settings.h"
#include "settings_ui.h"

#define WIDTH 1024
#define HEIGHT 600

static uint16_t s_frame[WIDTH * HEIGHT]; // what the panel would show, RGB565
static uint32_t s_now_ms;

static uint32_t tick(void)
{
    return s_now_ms;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    lv_display_flush_ready(display); // direct mode: LVGL drew into s_frame already
}

// Lets a little simulated time pass (timers, clock label) and renders.
static void render(void)
{
    for (int i = 0; i < 3; i++) {
        s_now_ms += 600;
        lv_timer_handler();
    }
    lv_refr_now(NULL);
}

static void save_bmp(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    // 24-bit BMP; a negative height stores rows top-down. Rows of 1024 pixels
    // need no padding.
    uint32_t size = 54 + WIDTH * HEIGHT * 3;
    uint8_t header[54] = {'B', 'M'};
    int32_t fields[] = {size, 0, 54, 40, WIDTH, -HEIGHT, 1 | 24 << 16, 0, WIDTH * HEIGHT * 3};
    memcpy(header + 2, fields, sizeof(fields)); // little-endian host
    fwrite(header, 1, sizeof(header), f);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        uint16_t p = s_frame[i];
        uint8_t bgr[3] = {(p & 0x1F) * 255 / 31, ((p >> 5) & 0x3F) * 255 / 63, (p >> 11) * 255 / 31};
        fwrite(bgr, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    rewind(f);
    char *data = malloc(*len + 1);
    if (fread(data, 1, *len, f) != *len) {
        perror(path);
        exit(1);
    }
    data[*len] = '\0';
    fclose(f);
    return data;
}

// net.h stand-ins: the simulator has no radio.
int net_scan(char ssids[][NET_SSID_LEN], int max)
{
    const char *fake[] = {"HomeNetwork", "Neighbours", "CoffeeShop"};
    int n = max < 3 ? max : 3;
    for (int i = 0; i < n; i++) {
        strcpy(ssids[i], fake[i]);
    }
    return n;
}

static void nothing(void) {}
static void nothing_saved(const settings_t *s) {}

int main(int argc, char **argv)
{
    if (argc != 7) {
        fprintf(stderr, "usage: %s <map.bin> <adsb.json> <lat> <lon> <range_nm> <out_dir>\n", argv[0]);
        return 2;
    }
    const char *out = argv[6];
    settings_t settings = {.ssid = "HomeNetwork", .range_nm = atoi(argv[5])};
    if (!settings_parse_coord(argv[3], -90, 90, &settings.lat) ||
        !settings_parse_coord(argv[4], -180, 180, &settings.lon) ||
        !settings_complete(&settings)) {
        fprintf(stderr, "bad centre or range (range is 25, 50, 100 or 250)\n");
        return 2;
    }

    size_t map_len, json_len;
    char *blob = read_file(argv[1], &map_len), *json = read_file(argv[2], &json_len);
    mapdata_t map;
    if (!mapdata_open(&map, blob, map_len)) {
        fprintf(stderr, "%s: not a valid map\n", argv[1]);
        return 1;
    }
    static adsb_aircraft_t list[ADSB_MAX_AIRCRAFT];
    int n = adsb_parse(json, settings.lat, settings.lon, list, ADSB_MAX_AIRCRAFT);
    if (n < 0) {
        fprintf(stderr, "%s: not an adsb.lol response\n", argv[2]);
        return 1;
    }

    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *display = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_buffers(display, s_frame, NULL, sizeof(s_frame), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, flush);

    radar_ui_create(&map, nothing);
    radar_ui_show(&settings);
    radar_ui_update(list, n);
    render();
    save_bmp(out, "radar.bmp");

    settings_ui_show(&map, &settings, nothing_saved);
    render();
    save_bmp(out, "settings.bmp");
    return 0;
}

#include <inttypes.h>

#include "adsb.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "mapdata.h"
#include "net.h"
#include "nvs_flash.h"
#include "radar_ui.h"
#include "settings.h"
#include "settings_ui.h"

#define POLL_PERIOD_MS 5000

static const char *TAG = "touchsky";

extern const uint8_t map_start[] asm("_binary_map_bin_start");
extern const uint8_t map_end[] asm("_binary_map_bin_end");

static mapdata_t s_map;
// Both written by LVGL callbacks and read by the poll task, under the display lock.
static settings_t s_settings;
static bool s_radar_active;

// Boot markers "TS_OK <subsystem>" are what tests/device/check_boot.py waits for.

static void on_touch(lv_event_t *e)
{
    static bool announced;
    if (!announced) {
        announced = true;
        ESP_LOGI(TAG, "TS_OK touch");
    }
}

static void start_radar(void)
{
    net_connect(s_settings.ssid, s_settings.pass);
    radar_ui_show(&s_settings);
    s_radar_active = true;
}

static void on_saved(const settings_t *s)
{
    s_settings = *s;
    settings_save(s);
    start_radar();
}

static void open_settings(void)
{
    s_radar_active = false;
    net_disconnect(); // a scan fails while the station is (re)connecting
    settings_ui_show(&s_map, &s_settings, on_saved);
}

static void *psram_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

static void poll_task(void *arg)
{
    static adsb_aircraft_t list[ADSB_MAX_AIRCRAFT];
    bool announced = false;
    for (;; vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS))) {
        bsp_display_lock(-1);
        settings_t s = s_settings;
        bool active = s_radar_active;
        bsp_display_unlock();
        if (!active) {
            continue;
        }

        int n = -1;
        const char *problem = "No Wi-Fi";
        if (net_connected()) {
            char url[96];
            adsb_build_url(url, sizeof(url), s.lat, s.lon, s.range_nm);
            const char *body = net_get(url);
            n = body ? adsb_parse(body, s.lat, s.lon, list, ADSB_MAX_AIRCRAFT) : -1;
            problem = "No data";
        }

        bsp_display_lock(-1);
        if (s_radar_active) { // the user may have opened the settings meanwhile
            if (n >= 0) {
                radar_ui_update(list, n);
            } else {
                radar_ui_offline(problem);
            }
        }
        bsp_display_unlock();
        if (n >= 0 && !announced) {
            announced = true;
            ESP_LOGI(TAG, "TS_OK adsb n=%d", n);
        }
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // A full-range answer is hundreds of kB of JSON: keep cJSON out of internal RAM.
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = psram_malloc, .free_fn = heap_caps_free});

    ESP_ERROR_CHECK(mapdata_open(&s_map, map_start, map_end - map_start) ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "TS_OK map lines=%" PRIu32 " airports=%" PRIu32, s_map.n_lines, s_map.n_airports);

    // Display rotated 180 degrees as in the Waveshare LVGL examples. Their touch
    // mirroring (mirror_x/y = 1) is left out: on this board it flips the touch
    // point to the opposite corner. Adjust touch_flags if a panel batch differs.
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    lv_display_t *display = bsp_display_start_with_config(&cfg);
    ESP_ERROR_CHECK(display != NULL ? ESP_OK : ESP_FAIL);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "TS_OK display %" PRId32 "x%" PRId32,
             lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));

    net_init();

    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);
    lv_indev_add_event_cb(bsp_display_get_input_dev(), on_touch, LV_EVENT_PRESSED, NULL);
    radar_ui_create(&s_map, open_settings);
    if (settings_load(&s_settings)) {
        start_radar();
    } else {
        open_settings();
    }
    bsp_display_unlock();

    xTaskCreate(poll_task, "adsb", 12288, NULL, 5, NULL);
}

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "touchsky";

// Boot markers "TS_OK <subsystem>" are what tests/device/check_boot.py waits for.

static void on_first_touch(lv_event_t *e)
{
    ESP_LOGI(TAG, "TS_OK touch");
    lv_obj_remove_event_cb(lv_event_get_target(e), on_first_touch);
}

void app_main(void)
{
    // Orientation and touch mapping as in the Waveshare LVGL examples for this panel.
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {.swap_xy = 0, .mirror_x = 1, .mirror_y = 1},
    };
    lv_display_t *display = bsp_display_start_with_config(&cfg);
    ESP_ERROR_CHECK(display != NULL ? ESP_OK : ESP_FAIL);
    bsp_display_backlight_on();

    ESP_ERROR_CHECK(bsp_display_lock(-1) ? ESP_OK : ESP_ERR_TIMEOUT);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "TouchSky");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_center(title);
    lv_obj_add_event_cb(screen, on_first_touch, LV_EVENT_PRESSED, NULL);
    bsp_display_unlock();

    ESP_LOGI(TAG, "TS_OK display %" PRId32 "x%" PRId32,
             lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
}

#include "settings_ui.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "net.h"

#define MAX_NETWORKS 12

static const int RANGES[] = {25, 50, 100, 250};

static const mapdata_t *s_map;
static void (*s_on_saved)(const settings_t *);
static lv_obj_t *s_screen, *s_keyboard, *s_error;
static lv_obj_t *s_ssid, *s_pass, *s_lat, *s_lon, *s_icao, *s_range;

static void on_focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(s_keyboard, lv_event_get_target(e));
    lv_keyboard_set_mode(s_keyboard, (lv_keyboard_mode_t)(uintptr_t)lv_event_get_user_data(e));
}

static void on_scan(lv_event_t *e)
{
    // ponytail: blocking scan under the display lock, the UI freezes for the
    // 2-3 s it takes. Move to a task if that becomes annoying.
    static char ssids[MAX_NETWORKS][NET_SSID_LEN];
    char options[MAX_NETWORKS * NET_SSID_LEN] = "";
    int n = net_scan(ssids, MAX_NETWORKS);
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            strlcat(options, "\n", sizeof(options));
        }
        strlcat(options, ssids[i], sizeof(options));
    }
    lv_dropdown_set_options(s_ssid, options);
    lv_label_set_text(s_error, n ? "" : "No Wi-Fi network found");
}

static void on_show_password(lv_event_t *e)
{
    lv_textarea_set_password_mode(s_pass, !lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static void on_lookup(lv_event_t *e)
{
    int i = mapdata_find_airport(s_map, lv_textarea_get_text(s_icao));
    if (i < 0) {
        lv_label_set_text(s_error, "Unknown airport code");
        return;
    }
    map_airport_t a = mapdata_airport(s_map, i);
    char text[16];
    snprintf(text, sizeof(text), "%.4f", a.lat / 1e6);
    lv_textarea_set_text(s_lat, text);
    snprintf(text, sizeof(text), "%.4f", a.lon / 1e6);
    lv_textarea_set_text(s_lon, text);
    lv_label_set_text(s_error, "");
}

static void on_save(lv_event_t *e)
{
    settings_t s = {.range_nm = RANGES[lv_dropdown_get_selected(s_range)]};
    lv_dropdown_get_selected_str(s_ssid, s.ssid, sizeof(s.ssid));
    strlcpy(s.pass, lv_textarea_get_text(s_pass), sizeof(s.pass));

    const char *error = NULL;
    if (s.ssid[0] == '\0') {
        error = "Pick a Wi-Fi network (Scan)";
    } else if (!settings_parse_coord(lv_textarea_get_text(s_lat), -90, 90, &s.lat)) {
        error = "Latitude must be between -90 and 90";
    } else if (!settings_parse_coord(lv_textarea_get_text(s_lon), -180, 180, &s.lon)) {
        error = "Longitude must be between -180 and 180";
    }
    if (error) {
        lv_label_set_text(s_error, error);
        return;
    }
    s_on_saved(&s);
    lv_obj_delete_async(s_screen);
}

static lv_obj_t *caption(const char *text, int y)
{
    lv_obj_t *label = lv_label_create(s_screen);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(label, 20, y + 12);
    return label;
}

static lv_obj_t *field(const char *placeholder, int x, int y, int w, lv_keyboard_mode_t mode)
{
    lv_obj_t *ta = lv_textarea_create(s_screen);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_width(ta, w);
    lv_obj_add_event_cb(ta, on_focus, LV_EVENT_FOCUSED, (void *)(uintptr_t)mode);
    return ta;
}

static void button(const char *text, int x, int y, int w, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(s_screen);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 46);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(b);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

void settings_ui_show(const mapdata_t *map, const settings_t *current,
                      void (*on_saved)(const settings_t *))
{
    s_map = map;
    s_on_saved = on_saved;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Three form rows on top, keyboard on the bottom half.
    caption("Wi-Fi", 20);
    s_ssid = lv_dropdown_create(s_screen);
    lv_obj_set_pos(s_ssid, 120, 20);
    lv_obj_set_width(s_ssid, 360);
    lv_dropdown_set_options(s_ssid, current->ssid);
    button("Scan", 490, 20, 110, on_scan);
    s_pass = field("Password", 620, 20, 290, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_textarea_set_password_mode(s_pass, true);
    lv_obj_t *show = lv_checkbox_create(s_screen);
    lv_checkbox_set_text(show, "Show");
    lv_obj_set_pos(show, 924, 32);
    lv_obj_add_event_cb(show, on_show_password, LV_EVENT_VALUE_CHANGED, NULL);
    lv_textarea_set_max_length(s_pass, sizeof(current->pass) - 1);
    lv_textarea_set_text(s_pass, current->pass);

    caption("Centre", 90);
    s_lat = field("Latitude", 120, 90, 180, LV_KEYBOARD_MODE_NUMBER);
    s_lon = field("Longitude", 310, 90, 180, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_x(caption("or", 90), 520);
    s_icao = field("ICAO", 570, 90, 140, LV_KEYBOARD_MODE_TEXT_UPPER);
    lv_textarea_set_max_length(s_icao, 4);
    button("Lookup", 720, 90, 130, on_lookup);

    caption("Range", 160);
    s_range = lv_dropdown_create(s_screen);
    lv_obj_set_pos(s_range, 120, 160);
    lv_obj_set_width(s_range, 180);
    lv_dropdown_set_options(s_range, "25 nm\n50 nm\n100 nm\n250 nm");
    s_error = lv_label_create(s_screen);
    lv_label_set_text(s_error, "");
    lv_obj_set_style_text_color(s_error, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_pos(s_error, 320, 172);
    button("Save", 874, 160, 130, on_save);

    s_keyboard = lv_keyboard_create(s_screen);
    lv_obj_set_size(s_keyboard, 1024, 360);

    if (settings_complete(current)) {
        char text[16];
        snprintf(text, sizeof(text), "%.4f", current->lat);
        lv_textarea_set_text(s_lat, text);
        snprintf(text, sizeof(text), "%.4f", current->lon);
        lv_textarea_set_text(s_lon, text);
        for (int i = 0; i < 4; i++) {
            if (RANGES[i] == current->range_nm) {
                lv_dropdown_set_selected(s_range, i);
            }
        }
    } else {
        lv_dropdown_set_selected(s_range, 1);
    }

    lv_screen_load(s_screen);
    if (current->ssid[0] == '\0') {
        on_scan(NULL); // first boot: list the networks straight away
    }
}

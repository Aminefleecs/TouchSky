// NVS side of settings.h (device only).
#include <string.h>

#include "esp_err.h"
#include "nvs.h"
#include "settings.h"

#define NAMESPACE "touchsky"
#define KEY "settings"

bool settings_load(settings_t *s)
{
    nvs_handle_t nvs;
    size_t size = sizeof(*s);
    bool ok = nvs_open(NAMESPACE, NVS_READONLY, &nvs) == ESP_OK;
    if (ok) {
        ok = nvs_get_blob(nvs, KEY, s, &size) == ESP_OK && size == sizeof(*s);
        nvs_close(nvs);
    }
    if (ok) {
        // Stored bytes are not trusted to be terminated.
        s->ssid[sizeof(s->ssid) - 1] = '\0';
        s->pass[sizeof(s->pass) - 1] = '\0';
        ok = settings_complete(s);
    }
    if (!ok) {
        memset(s, 0, sizeof(*s));
    }
    return ok;
}

void settings_save(const settings_t *s)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_blob(nvs, KEY, s, sizeof(*s)));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

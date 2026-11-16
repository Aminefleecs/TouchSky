#include "net.h"

#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"

// A 250 nm query over a busy area is a few hundred kB of JSON.
#define BODY_MAX (1024 * 1024)
#define SCAN_MAX 20

static const char *TAG = "net";
static bool s_connected, s_wanted;
static char *s_body;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        const wifi_event_sta_disconnected_t *d = data;
        ESP_LOGW(TAG, "disconnected from \"%.*s\", reason %d", d->ssid_len, (const char *)d->ssid, d->reason);
        if (s_wanted) {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        ESP_LOGI(TAG, "TS_OK wifi");
    }
}

void net_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));

    s_body = heap_caps_malloc(BODY_MAX, MALLOC_CAP_SPIRAM);
    ESP_ERROR_CHECK(s_body ? ESP_OK : ESP_ERR_NO_MEM);
}

void net_connect(const char *ssid, const char *pass)
{
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    net_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    s_wanted = true;
    ESP_LOGI(TAG, "connecting to \"%s\" (password: %d characters)", ssid, (int)strlen(pass));
    esp_wifi_connect();
}

void net_disconnect(void)
{
    s_wanted = false;
    esp_wifi_disconnect();
}

bool net_connected(void)
{
    return s_connected;
}

int net_scan(char ssids[][NET_SSID_LEN], int max)
{
    static wifi_ap_record_t records[SCAN_MAX]; // too big for the caller's stack
    uint16_t found = SCAN_MAX;
    if (esp_wifi_scan_start(NULL, true) != ESP_OK ||
        esp_wifi_scan_get_ap_records(&found, records) != ESP_OK) {
        return 0;
    }
    // Records come sorted by signal strength; an SSID seen on several access
    // points is kept once.
    int n = 0;
    for (int i = 0; i < found && n < max; i++) {
        const char *ssid = (const char *)records[i].ssid;
        bool skip = ssid[0] == '\0';
        for (int j = 0; j < n && !skip; j++) {
            skip = strcmp(ssids[j], ssid) == 0;
        }
        if (!skip) {
            strlcpy(ssids[n++], ssid, NET_SSID_LEN);
        }
    }
    return n;
}

const char *net_get(const char *url)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return NULL;
    }
    const char *result = NULL;
    if (esp_http_client_open(client, 0) == ESP_OK && esp_http_client_fetch_headers(client) >= 0) {
        int total = 0, n;
        while (total < BODY_MAX - 1 &&
               (n = esp_http_client_read(client, s_body + total, BODY_MAX - 1 - total)) > 0) {
            total += n;
        }
        s_body[total] = '\0';
        // A body that filled the buffer was cut short: treat it as an error
        // rather than parse truncated JSON.
        if (esp_http_client_get_status_code(client) == 200 && total < BODY_MAX - 1 &&
            esp_http_client_is_complete_data_received(client)) {
            result = s_body;
        } else {
            ESP_LOGW(TAG, "GET failed: status %d, %d bytes", esp_http_client_get_status_code(client), total);
        }
    } else {
        ESP_LOGW(TAG, "GET failed: no connection");
    }
    esp_http_client_cleanup(client);
    return result;
}

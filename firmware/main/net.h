// Wi-Fi station (through the ESP32-C6 coprocessor), SNTP and HTTPS GET.
#pragma once

#include <stdbool.h>

#define NET_SSID_LEN 33

// Starts Wi-Fi in station mode (not connected yet) and SNTP. Call once.
void net_init(void);

// Connects to this network and keeps reconnecting until net_disconnect().
void net_connect(const char *ssid, const char *pass);
void net_disconnect(void);
bool net_connected(void);

// Blocking scan (a few seconds). Fills ssids with up to max distinct,
// non-hidden network names, strongest first. Returns how many.
int net_scan(char ssids[][NET_SSID_LEN], int max);

// Blocking HTTPS GET. Returns the NUL-terminated body, or NULL on any error
// or non-200 status. The buffer is shared: valid until the next net_get().
const char *net_get(const char *url);

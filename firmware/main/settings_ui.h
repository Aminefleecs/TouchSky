// Settings screen: Wi-Fi network, radar centre (lat/lon or ICAO code), range.
// Must be called with the display lock held.
#pragma once

#include "mapdata.h"
#include "settings.h"

// Shows the screen, prefilled with *current. on_saved runs (display lock
// held) once the user saves a complete, valid set; the screen then deletes
// itself, so on_saved must load another screen.
void settings_ui_show(const mapdata_t *map, const settings_t *current,
                      void (*on_saved)(const settings_t *));

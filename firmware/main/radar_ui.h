// Radar screen: round scope on the left, side panel on the right.
// Every function must be called with the display lock held.
#pragma once

#include "adsb.h"
#include "mapdata.h"
#include "settings.h"

// Builds the screen (without showing it). on_settings runs when the
// Settings button is pressed.
void radar_ui_create(const mapdata_t *map, void (*on_settings)(void));

// Redraws the map for this centre and range, then shows the screen.
void radar_ui_show(const settings_t *settings);

// Fresh data: list is sorted by distance (as adsb_parse returns it).
void radar_ui_update(const adsb_aircraft_t *list, int count);

// No fresh data: the last aircraft stay on screen, greyed, with this reason.
void radar_ui_offline(const char *reason);

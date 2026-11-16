# TouchSky

ADS-B radar for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (7" 1024×600 touch display).
Shows the aircraft around a chosen point, using the [adsb.lol](https://api.adsb.lol/docs) API.

## Layout

| Path | Content |
|---|---|
| `firmware/` | ESP-IDF project (ESP-IDF v5.5.5, LVGL 9) |
| `tools/` | Host scripts (map data generation) |
| `tests/` | Host unit tests, tool tests, on-device checks (`tests/run.sh`) |
| `docs/` | Vendor documentation, not tracked (see below) |

## Toolchain

ESP-IDF v5.5.5 lives in `~/Developer/SDK/esp-idf`, its tools in `~/Developer/SDK/espressif`:

```sh
export IDF_TOOLS_PATH=~/Developer/SDK/espressif
. ~/Developer/SDK/esp-idf/export.sh
cd firmware && idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Plug the board through the USB-C port marked **UART** (USB-serial bridge), not the OTG one.

## Vendor documentation

Not committed (about 70 MB of PDF and CAD files). Sources:

- Board examples and BSP: <https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B>
- Board wiki, schematic, 3D files: <https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B>
- ESP32-P4 datasheet and technical reference manual: <https://www.espressif.com/en/support/documents/technical-documents>

idf_component_register(
    SRCS main.c geo.c adsb.c mapdata.c settings.c settings_store.c
         net.c radar_ui.c settings_ui.c
    INCLUDE_DIRS .
    EMBED_FILES map.bin)

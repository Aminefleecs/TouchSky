idf_component_register(
    SRCS main.c geo.c adsb.c mapdata.c settings.c
    INCLUDE_DIRS .
    EMBED_FILES map.bin)

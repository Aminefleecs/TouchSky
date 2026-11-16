# Host unit tests for the firmware's pure-C logic. Run through ../run.sh.
cmake_minimum_required(VERSION 3.16)
project(touchsky_host_tests C)

set(IDF "$ENV{IDF_PATH}")
if(NOT IDF)
    message(FATAL_ERROR "IDF_PATH is not set: load the ESP-IDF environment (export.sh) first")
endif()
set(FW "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/main")

add_compile_options(-Wall -Wextra)

# Unity and cJSON are the copies shipped with ESP-IDF, the ones the firmware uses.
add_library(unity "${IDF}/components/unity/unity/src/unity.c")
target_include_directories(unity PUBLIC "${IDF}/components/unity/unity/src")
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

add_library(logic
    "${FW}/geo.c" "${FW}/adsb.c" "${FW}/mapdata.c" "${FW}/settings.c"
    "${IDF}/components/json/cJSON/cJSON.c")
target_include_directories(logic PUBLIC "${FW}" "${IDF}/components/json/cJSON")
target_link_libraries(logic PUBLIC m)

# One executable per test_<module>.c, run from tests/ so fixtures/ paths resolve.
enable_testing()
file(GLOB TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")
foreach(src ${TESTS})
    get_filename_component(name "${src}" NAME_WE)
    add_executable(${name} "${src}")
    target_link_libraries(${name} logic unity)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
endforeach()

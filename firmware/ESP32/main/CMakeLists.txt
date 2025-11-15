FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS "main.c"  "openhaystack_main.c" REQUIRES bt nvs_flash INCLUDE_DIRS ".")
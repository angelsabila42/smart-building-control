# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "main.c"
        "rfid.c"
        "access_control.c"
        "Rgb/rgb.c"
        "Serv/serv.c"
        "Sdcard/sd_card.c"
    INCLUDE_DIRS
        "."
        "Rgb"
        "Serv"
        "Sdcard"
        "Accesscontrol"
)

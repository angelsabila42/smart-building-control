# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "main.c"
        "rfid.c"
        "Accesscontrol/access_control.c"
        "Rgb/rgb.c"
        "Serv/serv.c"
        "Sdcard/sd_card.c"
        "management/time_utils.c"
        "management/sha256.c"
        "management/access_schedule.c"
        "management/admin_auth.c"
        "management/access_events.c"
        "management/system_status.c"
        "management/serial_menu.c"
        "management/management_app.c"
        "management/management_console.c"
        "management/management_store.c"
    INCLUDE_DIRS
        "."
        "Rgb"
        "Serv"
        "Sdcard"
        "Accesscontrol"
)

cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    message(FATAL_ERROR "CMAKE_SYSTEM_NAME must be WindowsStore")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(../cmake/bin2c.cmake)
include(../cmake/GetGitRevisionDescription.cmake)
git_describe(ARDENS_VERSION)
message(STATUS "ARDENS_VERSION: ${ARDENS_VERSION}")
string(REGEX REPLACE "^v" "" ARDENS_VERSION_SHORT ${ARDENS_VERSION})
string(REGEX REPLACE "-.*" "" ARDENS_VERSION_SHORT ${ARDENS_VERSION_SHORT})
message(STATUS "ARDENS_VERSION_SHORT: ${ARDENS_VERSION_SHORT}")

project(Ardens_UWP)

file(GLOB TARGETS "targets/*.arduboy")
file(GLOB DATA_FILES "data/*")

add_subdirectory(../deps/fmt "${CMAKE_CURRENT_BINARY_DIR}/UWP/fmt")
add_subdirectory(../deps/miniz "${CMAKE_CURRENT_BINARY_DIR}/UWP/miniz")
add_subdirectory(../deps/bitsery "${CMAKE_CURRENT_BINARY_DIR}/UWP/bitsery")
add_subdirectory(../deps/SDL "${CMAKE_CURRENT_BINARY_DIR}/UWP/SDL" EXCLUDE_FROM_ALL)

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
set(DEPDIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps")

set(ARDENS_SOURCES
    ${SRCDIR}/absim.hpp
    ${SRCDIR}/absim_instructions.hpp
    ${SRCDIR}/absim_cpu_data.cpp
    ${SRCDIR}/absim_arduboy.cpp
    ${SRCDIR}/absim_ssd1306.hpp
    ${SRCDIR}/absim_atmega32u4.hpp
    ${SRCDIR}/absim_w25q128.hpp
    ${SRCDIR}/absim_decode.cpp
    ${SRCDIR}/absim_merge_instrs.cpp
    ${SRCDIR}/absim_execute.cpp
    ${SRCDIR}/absim_execute_merged.cpp
    ${SRCDIR}/absim_disassemble.cpp
    ${SRCDIR}/absim_load_file.cpp
    ${SRCDIR}/absim_reset.cpp
    ${SRCDIR}/absim_snapshot.cpp
    ${SRCDIR}/absim_timer.hpp
    ${SRCDIR}/absim_adc.hpp
    ${SRCDIR}/absim_pll.hpp
    ${SRCDIR}/absim_spi.hpp
    ${SRCDIR}/absim_eeprom.hpp
    ${SRCDIR}/absim_sound.hpp
    ${SRCDIR}/absim_led.cpp
    ${SRCDIR}/absim_usb.cpp

    ${DEPDIR}/imgui/imgui_impl_sdl3.h
    ${DEPDIR}/imgui/imgui_impl_sdl3.cpp
    ${DEPDIR}/imgui/imgui_impl_sdlrenderer3.h
    ${DEPDIR}/imgui/imgui_impl_sdlrenderer3.cpp
    ${DEPDIR}/imgui/imconfig.h
    ${DEPDIR}/imgui/imgui_internal.h
    ${DEPDIR}/imgui/imgui.h
    ${DEPDIR}/imgui/imgui.cpp
    ${DEPDIR}/imgui/imgui_draw.cpp
    ${DEPDIR}/imgui/imgui_tables.cpp
    ${DEPDIR}/imgui/imgui_widgets.cpp
    ${DEPDIR}/imgui/imstb_rectpack.h
    ${DEPDIR}/imgui/imstb_textedit.h
    ${DEPDIR}/imgui/imstb_truetype.h
    ${DEPDIR}/imgui/imgui_memory_editor.h
    
    #${SRCDIR}/settings.hpp
    #${SRCDIR}/settings.cpp
    #${SRCDIR}/common.hpp
    #${SRCDIR}/common.cpp
    #${SRCDIR}/view_player.cpp
    #${SRCDIR}/scalenx.cpp
    #${SRCDIR}/saveload.cpp
    
    #${SRCDIR}/main_sdl.cpp

    main.cpp
)

macro(add_uwp_target ARG_TARGET_NAME ARG_ARDUBOY_FILE ARG_PACKAGE_ID ARG_STORE_ID ARG_APP_VERSION)

    set(TARGET_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ARG_TARGET_NAME}")
    set(APPXMANIFEST "${TARGET_BINARY_DIR}/Package.appxmanifest")
    file(MAKE_DIRECTORY "${TARGET_BINARY_DIR}")
    
    set(PACKAGE_ID "${ARG_PACKAGE_ID}")
    set(APP_VERSION "${ARG_APP_VERSION}")
    set(SHORT_NAME "${ARG_TARGET_NAME}")
    set(PUBLISHER "CN=B2D11BC5-37CB-4400-B5C4-EA506EFE1371")
    set(PUBLISHER_NAME "Peter Brown")
    set(PHONE_PUBLISHER "7b792339-03b0-44b8-9b3d-21cc9495c732")
    set(PHONE_GUID "00000000-0000-0000-0000-000000000000")

    configure_file(
        Package.appxmanifest.in
        "${APPXMANIFEST}"
        @ONLY
    )

    set(ARDUBOY_H "${TARGET_BINARY_DIR}/distgame.h")
    set(ARDUBOY_C "${TARGET_BINARY_DIR}/distgame.c")
    bin2c(
        "${ARDUBOY_C}"
        "${ARDUBOY_H}"
        "${ARG_ARDUBOY_FILE}"
        "game_arduboy"
    )
    
    add_executable(${ARG_TARGET_NAME} WIN32
        ${ARDENS_SOURCES}
        "${APPXMANIFEST}"
        "${ARDUBOY_C}"
        "${ARDUBOY_H}"
        ${DATA_FILES}
    )

    target_include_directories(${ARG_TARGET_NAME} PRIVATE
        ../src
        ../deps
        ../deps/imgui
        ../deps/rapidjson/include
        "${TARGET_BINARY_DIR}"
    )

    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_VERSION="${ARDENS_VERSION}")
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_PLAYER)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_NO_SNAPSHOTS)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_NO_DEBUGGER)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_NO_GUI)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_NO_SAVED_SETTINGS)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_NO_SCALING)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_PLATFORM_SDL)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_DIST)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -DARDENS_UWP)

    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -D_CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(${ARG_TARGET_NAME} PRIVATE -D_SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING)

    target_compile_options(${ARG_TARGET_NAME} PRIVATE "/ZW" "/EHsc" "/Zc:__cplusplus")
    target_link_libraries(${ARG_TARGET_NAME} PRIVATE bitsery miniz fmt SDL3::SDL3)

endmacro()

add_uwp_target(
    "DazzleDash"
    "targets/DazzleDash.arduboy"
    "27355PeterBrown.DazzleDash"
    "9NML9NWHX8N5"
    "1.0.0.0"
)

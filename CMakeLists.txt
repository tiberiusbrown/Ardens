cmake_minimum_required(VERSION 3.16)

if(MSVC)
    foreach(flag
        CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG_INIT
        CMAKE_CXX_FLAGS_RELEASE  CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_DEBUG  CMAKE_CXX_FLAGS_DEBUG_INIT)
        string(REPLACE "/MD"  "/MT" "${flag}" "${${flag}}")
        set("${flag}" "${${flag}} /EHsc")
    endforeach()
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        a1d1946dcba6509f0679f507b57e7b228d32e6f8 # 2.24.1
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        9.1.0
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(fmt)

project(arduboy_sim)
set(EXEFLAG)
if(MSVC)
    set(EXEFLAG WIN32)
endif()
add_executable(arduboy_sim ${EXEFLAG}
    src/main.cpp
    src/window_disassembly.cpp
    src/window_profiler.cpp
    src/window_display.cpp
    src/window_display_internals.cpp
    src/window_data_space.cpp

    src/absim.hpp
    src/absim_instructions.hpp

    src/absim_ssd1306.cpp
    src/absim_atmega32u4.cpp
    src/absim_timer.cpp
    src/absim_pll.cpp
    src/absim_spi.cpp
    src/absim_eeprom.cpp
    src/absim_reset.cpp
    src/absim_decode.cpp
    src/absim_execute.cpp
    src/absim_disassemble.cpp
    src/absim_arduboy.cpp
    src/absim_load_file.cpp
    
    src/imgui_impl_sdl.h
    src/imgui_impl_sdl.cpp
    src/imgui_impl_sdlrenderer.h
    src/imgui_impl_sdlrenderer.cpp
    
    src/imconfig.h
    src/imgui_internal.h
    src/imgui.h
    src/imgui.cpp
    src/imgui_demo.cpp
    src/imgui_draw.cpp
    src/imgui_tables.cpp
    src/imgui_widgets.cpp
    src/imstb_rectpack.h
    src/imstb_textedit.h
    src/imstb_truetype.h

    src/imgui_memory_editor.h
    )
target_link_libraries(arduboy_sim SDL2-static SDL2main fmt::fmt)

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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(arduboy_sim)

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
    ELFIO
    GIT_REPOSITORY https://github.com/serge1/ELFIO.git
    GIT_TAG        Release_3.11
    GIT_PROGRESS   TRUE
)
set(ELFIO_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(ELFIO)

set(EXEFLAG)
if(MSVC)
    set(EXEFLAG WIN32)
endif()
set(IMGUI_SOURCES
    src/imgui/imgui_impl_sdl.h
    src/imgui/imgui_impl_sdl.cpp
    src/imgui/imgui_impl_sdlrenderer.h
    src/imgui/imgui_impl_sdlrenderer.cpp
    src/imgui/imconfig.h
    src/imgui/imgui_internal.h
    src/imgui/imgui.h
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imstb_rectpack.h
    src/imgui/imstb_textedit.h
    src/imgui/imstb_truetype.h
    src/imgui/imgui_memory_editor.h
    )
set(LLVM_DEMANGLER_SOURCES
    src/llvm_demangler/cxa_demangle.cpp
    src/llvm_demangler/__cxxabi_config.h
    src/llvm_demangler/cxa_demangle.cpp
    src/llvm_demangler/demangle/DemangleConfig.h
    src/llvm_demangler/demangle/ItaniumDemangle.h
    src/llvm_demangler/demangle/StringView.h
    src/llvm_demangler/demangle/Utility.h
    )
add_executable(arduboy_sim ${EXEFLAG}
    src/main.cpp
    src/window_disassembly.cpp
    src/window_profiler.cpp
    src/window_display.cpp
    src/window_display_internals.cpp
    src/window_data_space.cpp
    src/window_simulation.cpp

    src/absim.hpp
    src/absim_instructions.hpp

    src/absim_arduboy.cpp
    src/absim_ssd1306.hpp
    src/absim_atmega32u4.hpp
    src/absim_decode.cpp
    src/absim_execute.hpp
    src/absim_disassemble.cpp
    src/absim_load_file.cpp
    src/absim_reset.cpp

    src/absim_timer.hpp
    src/absim_adc.hpp
    src/absim_pll.hpp
    src/absim_spi.hpp
    src/absim_eeprom.hpp
    
    ${IMGUI_SOURCES}
    ${LLVM_DEMANGLER_SOURCES}
    )
target_link_libraries(arduboy_sim SDL2-static SDL2main elfio)
target_include_directories(arduboy_sim PUBLIC
    src/imgui
    src/libelfin
    src/llvm_demangler
    )
target_compile_definitions(arduboy_sim PRIVATE -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS)
source_group("ImGui" FILES ${IMGUI_SOURCES})
source_group("LLVM Demangler" FILES ${LLVM_DEMANGLER_SOURCES})

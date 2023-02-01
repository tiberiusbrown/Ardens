cmake_minimum_required(VERSION 3.18)

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
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.26.2/SDL2-2.26.2.tar.gz
)
FetchContent_MakeAvailable(SDL2)
set_property(DIRECTORY ${SDL2_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)

FetchContent_Declare(
    ELFIO
    URL https://github.com/serge1/ELFIO/releases/download/Release_3.11/elfio-3.11.tar.gz
)
set(ELFIO_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(ELFIO)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    llvm
    URL            https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/llvm-15.0.7.src.tar.xz
)
set(LLVM_USE_CRT_DEBUG          "MTd")
set(LLVM_USE_CRT_MINSIZEREL     "MT")
set(LLVM_USE_CRT_RELEASE        "MT")
set(LLVM_USE_CRT_RELWITHDEBINFO "MT")
set(LLVM_BUILD_TESTS            OFF)
set(LLVM_BUILD_TOOLS            OFF)
set(LLVM_BUILD_UTILS            OFF)
set(LLVM_INCLUDE_TESTS          OFF)
set(LLVM_INCLUDE_TOOLS          OFF)
set(LLVM_INCLUDE_UTILS          OFF)
set(LLVM_INCLUDE_BENCHMARKS     OFF)
set(LLVM_INCLUDE_DOCS           OFF)
set(LLVM_INCLUDE_EXAMPLES       OFF)
set(LLVM_INCLUDE_RUNTIMES       OFF)
set(LLVM_TARGETS_TO_BUILD ";" CACHE STRING "" FORCE)
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD ";" CACHE STRING "" FORCE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
FetchContent_MakeAvailable(llvm)
set_property(DIRECTORY ${llvm_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)

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
    src/absim_cpu_data.cpp

    src/absim_arduboy.cpp
    src/absim_ssd1306.hpp
    src/absim_atmega32u4.hpp
    src/absim_w25q128.hpp

    src/absim_decode.cpp
    src/absim_execute.cpp
    src/absim_execute_merged.cpp
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
target_link_libraries(arduboy_sim PRIVATE
    SDL2-static
    SDL2main
    elfio
    nlohmann_json::nlohmann_json
    LLVMDebugInfoDWARF
    LLVMDemangle
    )
target_include_directories(arduboy_sim PRIVATE
    src/imgui
    src/libelfin
    src/miniz-cpp
    "$<TARGET_PROPERTY:LLVMDebugInfoDWARF,INCLUDE_DIRECTORIES>"
    "$<TARGET_PROPERTY:LLVMDemangle,INCLUDE_DIRECTORIES>"
    )
source_group("ImGui" FILES ${IMGUI_SOURCES})

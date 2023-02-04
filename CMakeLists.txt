cmake_minimum_required(VERSION 3.18)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(arduboy_sim)

if(EMSCRIPTEN)
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -Oz")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -flto")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -fno-rtti -fno-exceptions")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${EXTRA_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS}")
    add_link_options("--closure 1")
    add_link_options("-s MODULARIZE=1")
    #add_link_options("-s TOTAL_MEMORY=24MB")
    add_link_options("-sEXPORTED_FUNCTIONS=_main,_load_file,_malloc,_free")
    add_link_options("-sEXPORTED_RUNTIME_METHODS=ccall")
    add_link_options("-flto")
endif()

add_subdirectory(deps/SDL)
set_property(DIRECTORY deps/SDL PROPERTY EXCLUDE_FROM_ALL YES)

add_subdirectory(deps/json)

add_subdirectory(deps/miniz)

if(MSVC)
    set(LLVM_USE_CRT_DEBUG          "MTd" CACHE STRING "" FORCE)
    set(LLVM_USE_CRT_MINSIZEREL     "MT"  CACHE STRING "" FORCE)
    set(LLVM_USE_CRT_RELEASE        "MT"  CACHE STRING "" FORCE)
    set(LLVM_USE_CRT_RELWITHDEBINFO "MT"  CACHE STRING "" FORCE)
    set(LLVM_BUILD_LLVM_C_DYLIB     OFF   CACHE BOOL   "" FORCE)
endif()
set(LLVM_BUILD_TESTS                   OFF CACHE BOOL   "" FORCE)
set(LLVM_BUILD_TOOLS                   OFF CACHE BOOL   "" FORCE)
set(LLVM_BUILD_UTILS                   OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_TESTS                 OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_TOOLS                 OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_UTILS                 OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_BENCHMARKS            OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_DOCS                  OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_EXAMPLES              OFF CACHE BOOL   "" FORCE)
set(LLVM_INCLUDE_RUNTIMES              OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_BINDINGS               OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_LIBEDIT                OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_LIBPFM                 OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_OCAMLDOC               OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_TIMESTAMPS             OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_TERMINFO               OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_ZLIB                   OFF CACHE BOOL   "" FORCE)
set(LLVM_ENABLE_LIBXML2                OFF CACHE BOOL   "" FORCE)
set(_ENABLE_LLVM_NATIVE_BUILD          OFF CACHE BOOL   "" FORCE)
set(LLVM_TARGETS_TO_BUILD              ";" CACHE STRING "" FORCE)
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD ";" CACHE STRING "" FORCE)
if(EMSCRIPTEN)
    set(LLVM_TARGET_ARCH "wasm64" CACHE STRING "" FORCE)
    set(LLVM_NATIVE_ARCH "wasm64" CACHE STRING "" FORCE)
    set(LLVM_HOST_TRIPLE "wasm64-unknown-emscripten" CACHE STRING "" FORCE)
    set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(deps/llvm-project/llvm)
set_property(DIRECTORY deps/llvm-project/llvm PROPERTY EXCLUDE_FROM_ALL YES)

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
    .editorconfig

    src/main.cpp
    src/window_disassembly.cpp
    src/window_profiler.cpp
    src/window_display.cpp
    src/window_display_internals.cpp
    src/window_data_space.cpp
    src/window_simulation.cpp
    src/window_call_stack.cpp

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
    )
target_link_libraries(arduboy_sim PRIVATE
    SDL2-static
    SDL2main
    nlohmann_json::nlohmann_json
    LLVMDebugInfoDWARF
    LLVMDemangle
    LLVMObject
    miniz
    )
target_include_directories(arduboy_sim PRIVATE
    src/imgui
    #src/miniz-cpp
    "$<TARGET_PROPERTY:LLVMDebugInfoDWARF,INCLUDE_DIRECTORIES>"
    "$<TARGET_PROPERTY:LLVMDemangle,INCLUDE_DIRECTORIES>"
    "$<TARGET_PROPERTY:LLVMObject,INCLUDE_DIRECTORIES>"
    )
source_group("ImGui" FILES ${IMGUI_SOURCES})
if(EMSCRIPTEN)
    target_link_options(arduboy_sim PRIVATE -s TOTAL_MEMORY=24MB)
endif()

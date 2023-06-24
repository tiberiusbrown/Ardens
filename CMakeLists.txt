cmake_minimum_required(VERSION 3.15)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(arduboy_sim)

if(MSVC)
    add_link_options(
        /DEBUG
        /INCREMENTAL:NO                     # Disable incremental linking.
        $<$<NOT:$<CONFIG:DEBUG>>:/OPT:REF>  # Remove unreferenced functions and data.
        $<$<NOT:$<CONFIG:DEBUG>>:/OPT:ICF>  # Identical COMDAT folding.
        $<$<NOT:$<CONFIG:DEBUG>>:/LTCG>     # Link-Time Code Generation.
        $<$<CONFIG:DEBUG>:/OPT:NOREF>       # No unreferenced data elimination.
        $<$<CONFIG:DEBUG>:/OPT:NOICF>       # No Identical COMDAT folding.
    )
    add_compile_options(
        $<$<NOT:$<CONFIG:DEBUG>>:/GL>
        $<$<NOT:$<CONFIG:DEBUG>>:/Gy>
        $<$<NOT:$<CONFIG:DEBUG>>:/Gw>
        $<$<NOT:$<CONFIG:DEBUG>>:/GF>
        $<$<NOT:$<CONFIG:DEBUG>>:/Oy>
        $<$<CONFIG:RELEASE>:/Ot>
        $<$<CONFIG:MINSIZEREL>:/Os>
        $<$<CONFIG:RELEASE>:/Ox>
    )
endif()

option(ABSIM_LLVM "Build and link LLVM (enables loading ELF and debug info)" ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GetGitRevisionDescription)
git_describe(ABSIM_VERSION)
message(STATUS "ABSIM_VERSION: ${ABSIM_VERSION}")

if(EMSCRIPTEN)
    #set(EXTRA_FLAGS "${EXTRA_FLAGS} -O0 -g3 -gsource-map")
    #add_link_options("-gsource-map")
    #add_link_options("--profiling")
    #add_link_options("--profiling-funcs")
    #add_link_options("--cpuprofiler")
    
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -O3")
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -flto")
    add_link_options("--closure 1")
    add_link_options("-s MODULARIZE=1")
    add_link_options("-flto")
    
    set(EXTRA_FLAGS "${EXTRA_FLAGS} -fno-rtti -fno-exceptions")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${EXTRA_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS}")
    add_link_options("-sALLOW_MEMORY_GROWTH")
    add_link_options("-sINITIAL_MEMORY=24MB")
    add_link_options("-sEXPORTED_FUNCTIONS=_main,_load_file,_malloc,_free,_postsyncfs,_setparam")
    add_link_options("-sEXPORTED_RUNTIME_METHODS=ccall,FS")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$autoResumeAudioContext','$dynCall']")
endif()

add_subdirectory(deps/SDL)
set_property(DIRECTORY deps/SDL PROPERTY EXCLUDE_FROM_ALL YES)

add_subdirectory(deps/json)
add_subdirectory(deps/fmt)
add_subdirectory(deps/miniz)
add_subdirectory(deps/bitsery)

if(ABSIM_LLVM)
    if(MSVC)
        set(LLVM_USE_CRT_DEBUG          "MTd" CACHE STRING "" FORCE)
        set(LLVM_USE_CRT_MINSIZEREL     "MT"  CACHE STRING "" FORCE)
        set(LLVM_USE_CRT_RELEASE        "MT"  CACHE STRING "" FORCE)
        set(LLVM_USE_CRT_RELWITHDEBINFO "MT"  CACHE STRING "" FORCE)
        set(LLVM_BUILD_LLVM_C_DYLIB     OFF   CACHE BOOL   "" FORCE)
    endif()
    set(LLVM_BUILD_TESTS                   OFF         CACHE BOOL   "" FORCE)
    set(LLVM_BUILD_TOOLS                   ON          CACHE BOOL   "" FORCE)
    set(LLVM_BUILD_UTILS                   ON          CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_TESTS                 OFF         CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_TOOLS                 ON          CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_UTILS                 ON          CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_BENCHMARKS            OFF         CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_DOCS                  OFF         CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_EXAMPLES              OFF         CACHE BOOL   "" FORCE)
    set(LLVM_INCLUDE_RUNTIMES              OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_BINDINGS               OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_LIBEDIT                OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_LIBPFM                 OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_OCAMLDOC               OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_TIMESTAMPS             OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_TERMINFO               OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_PIC                    OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_THREADS                OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_UNWIND_TABLES          OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_ZLIB                   "OFF"       CACHE STRING "" FORCE)
    set(LLVM_ENABLE_ZSTD                   "OFF"       CACHE STRING "" FORCE)
    set(LLVM_ENABLE_LIBXML2                OFF         CACHE BOOL   "" FORCE)
    #set(LLVM_ENABLE_LTO                    "Full"      CACHE STRING "" FORCE)
    #set(LLVM_TARGET_ARCH                   "AVR"       CACHE STRING "" FORCE)
    set(LLVM_DEFAULT_TARGET_TRIPLE         "avr"       CACHE STRING "" FORCE)
    set(_ENABLE_LLVM_NATIVE_BUILD          OFF         CACHE BOOL   "" FORCE)
    set(LLVM_ENABLE_PROJECTS               "clang;lld" CACHE STRING "" FORCE)
    set(LLVM_TARGETS_TO_BUILD              ";"         CACHE STRING "" FORCE)
    set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD "AVR"       CACHE STRING "" FORCE)
    set(CLANG_ENABLE_ARCMT                 OFF         CACHE BOOL   "" FORCE)
    set(CLANG_ENABLE_STATIC_ANALYZER       OFF         CACHE BOOL   "" FORCE)
    set(CLANG_BUILD_TOOLS                  OFF         CACHE BOOL   "" FORCE)
    set(CLANG_TOOL_HANDLE_CXX_BUILD        OFF         CACHE BOOL   "" FORCE)
    set(CLANG_TOOL_HANDLE_LLVM_BUILD       OFF         CACHE BOOL   "" FORCE)
    set(LLD_BUILD_TOOLS                    OFF         CACHE BOOL   "" FORCE)
    if(EMSCRIPTEN)
        #set(LLVM_TARGET_ARCH "wasm64" CACHE STRING "" FORCE)
        set(LLVM_NATIVE_ARCH "wasm64" CACHE STRING "" FORCE)
        set(LLVM_HOST_TRIPLE "wasm64-unknown-emscripten" CACHE STRING "" FORCE)
        set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(deps/llvm-project/llvm)
    set_property(DIRECTORY deps/llvm-project/llvm PROPERTY EXCLUDE_FROM_ALL YES)
endif()

set(EXEFLAG)
if(MSVC)
    set(EXEFLAG WIN32)
endif()
set(IMGUI_SOURCES
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
set(IMGUI_SDL_SOURCES
    src/imgui/imgui_impl_sdl.h
    src/imgui/imgui_impl_sdl.cpp
    src/imgui/imgui_impl_sdlrenderer.h
    src/imgui/imgui_impl_sdlrenderer.cpp
    )
set(IMPLOT_SOURCES
    src/implot/implot.h
    src/implot/implot.cpp
    src/implot/implot_internal.h
    #src/implot/implot_demo.cpp
    src/implot/implot_items.cpp
    )
set(GIFENC_SOURCES
    src/gifenc/gifenc.h
    src/gifenc/gifenc.c
    )
set(SOKOL_SOURCES
    src/sokol/sokol_app.h
    src/sokol/sokol_audio.h
    src/sokol/sokol_fetch.h
    src/sokol/sokol_gfx.h
    src/sokol/sokol_glue.h
    src/sokol/sokol_imgui.h
    src/sokol/sokol_time.h
    )
set(SIMPLE_FFT_SOURCES
    src/Simple-FFT/include/simple_fft/check_fft.hpp
    src/Simple-FFT/include/simple_fft/copy_array.hpp
    src/Simple-FFT/include/simple_fft/error_handling.hpp
    src/Simple-FFT/include/simple_fft/fft.h
    src/Simple-FFT/include/simple_fft/fft.hpp
    src/Simple-FFT/include/simple_fft/fft_impl.hpp
    src/Simple-FFT/include/simple_fft/fft_settings.h
    )
set(HQX_SOURCES
    src/hqx/include/hqx/HQx.hh
    src/hqx/include/hqx/HQ2x.hh
    src/hqx/include/hqx/HQ3x.hh
    src/hqx/source/HQx.cc
    src/hqx/source/HQ2x.cc
    src/hqx/source/HQ3x.cc
    )
set(ABSIM_SOURCES
    src/absim.hpp
    src/absim_instructions.hpp
    src/absim_cpu_data.cpp

    src/absim_arduboy.cpp
    src/absim_ssd1306.hpp
    src/absim_atmega32u4.hpp
    src/absim_w25q128.hpp

    src/absim_decode.cpp
    src/absim_merge_instrs.cpp
    src/absim_execute.cpp
    src/absim_execute_merged.cpp
    src/absim_disassemble.cpp
    src/absim_load_file.cpp
    src/absim_reset.cpp
    src/absim_snapshot.cpp

    src/absim_timer.hpp
    src/absim_adc.hpp
    src/absim_pll.hpp
    src/absim_spi.hpp
    src/absim_eeprom.hpp
    src/absim_sound.hpp
    src/absim_led.cpp
    src/absim_usb.cpp
    )
set(EM_FILE_BROWSER_SOURCES)
if(EMSCRIPTEN)
    set(EM_FILE_BROWSER_SOURCES src/emscripten-browser-file/emscripten_browser_file.h)
endif()

set_source_files_properties(
    src/absim_snapshot.cpp
    src/view_debugger.cpp
    src/common.cpp
    PROPERTIES
        COMPILE_OPTIONS "-DABSIM_VERSION=\"${ABSIM_VERSION}\""
    )

add_executable(arduboy_sim ${EXEFLAG}
    .editorconfig

    src/main_sdl.cpp
    src/main_sokol.cpp

    src/font.cpp
    src/settings.hpp
    src/settings.cpp
    src/settings_modal.cpp
    src/modal_about.cpp
    src/common.hpp
    src/common.cpp
    src/view_debugger.cpp
    src/view_player.cpp
    src/record_gif.cpp
    src/record_wav.cpp
    src/scalenx.cpp
    src/saveload.cpp
    src/dwarf.hpp
    src/dwarf.cpp
    src/dwarf_expr.cpp
    src/clang.hpp
    src/clang.cpp

    src/window_disassembly.cpp
    src/window_profiler.cpp
    src/window_display.cpp
    src/window_display_buffer.cpp
    src/window_display_internals.cpp
    src/window_data_space.cpp
    src/window_progmem.cpp
    src/window_simulation.cpp
    src/window_call_stack.cpp
    src/window_symbols.cpp
    src/window_globals.cpp
    src/window_fx_data.cpp
    src/window_fx_internals.cpp
    src/window_eeprom.cpp
    src/window_cpu_usage.cpp
    src/window_led.cpp
    src/window_serial.cpp
    src/window_sound.cpp
    src/window_source.cpp

    ${ABSIM_SOURCES}

    src/stb_image_write.h
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
    ${GIFENC_SOURCES}
    ${EM_FILE_BROWSER_SOURCES}
    ${SIMPLE_FFT_SOURCES}
    ${HQX_SOURCES}
    ${SOKOL_SOURCES}

    src/ImGuiColorTextEdit/TextEditor.h
    src/ImGuiColorTextEdit/TextEditor.cpp

    #${IMGUI_SDL_SOURCES}
    )
target_link_libraries(arduboy_sim PRIVATE
    #SDL2-static
    #SDL2main
    nlohmann_json::nlohmann_json
    miniz
    fmt
    bitsery
    )
if(ABSIM_LLVM)
    set(LLVM_LIBS
        LLVMDebugInfoDWARF
        LLVMDemangle
        LLVMObject
        LLVMAVRCodeGen
        clangCodeGen
        lldCommon
        lldELF
        )
    foreach(L ${LLVM_LIBS})
        target_link_libraries(arduboy_sim PRIVATE ${L})
        target_include_directories(arduboy_sim PRIVATE "$<TARGET_PROPERTY:${L},INCLUDE_DIRECTORIES>")
    endforeach()
    target_compile_definitions(arduboy_sim PRIVATE -DABSIM_LLVM)
endif()
#target_compile_definitions(arduboy_sim PRIVATE -DABSIM_PLATFORM_SDL)
target_compile_definitions(arduboy_sim PRIVATE -DABSIM_PLATFORM_SOKOL)
target_include_directories(arduboy_sim PRIVATE
    src/imgui
    src/implot
    src/gifenc
    src/Simple-FFT/include
    src/hqx/include
    )

add_executable(arduboy_sim_player ${EXEFLAG}
    .editorconfig

    src/main_sdl.cpp
    src/main_sokol.cpp

    src/settings.hpp
    src/settings.cpp
    src/common.hpp
    src/common.cpp
    src/view_player.cpp
    src/record_gif.cpp
    src/record_wav.cpp
    src/scalenx.cpp
    src/saveload.cpp
    src/dwarf.hpp
    src/dwarf.cpp

    ${ABSIM_SOURCES}

    src/stb_image_write.h
    ${IMGUI_SOURCES}
    ${GIFENC_SOURCES}
    ${EM_FILE_BROWSER_SOURCES}
    ${SOKOL_SOURCES}
    )
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_PLAYER)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_SNAPSHOTS)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_DEBUGGER)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_GUI)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_SAVED_SETTINGS)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_SCALING)
#target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_NO_ARDUBOY_FILE)
target_compile_definitions(arduboy_sim_player PRIVATE -DABSIM_PLATFORM_SOKOL)
target_include_directories(arduboy_sim_player PRIVATE
    src/imgui
    src/gifenc
    )
target_link_libraries(arduboy_sim_player PRIVATE
    nlohmann_json::nlohmann_json
    miniz
    fmt
    bitsery
    )

if(MSVC)
    target_compile_options(arduboy_sim PRIVATE /Zc:__cplusplus)
    target_compile_options(arduboy_sim_player PRIVATE /Zc:__cplusplus)
endif()

if(EMSCRIPTEN)
    target_link_options(arduboy_sim PRIVATE "-lidbfs.js")
    target_link_options(arduboy_sim_player PRIVATE "-lidbfs.js")
endif()

#if(MSVC)
#    include(CheckIPOSupported)
#    check_ipo_supported(RESULT ipo_result OUTPUT ipo_err)
#    if(ipo_result)
#        message(STATUS "IPO is supported")
#        set_property(
#            TARGET
#                arduboy_sim
#                arduboy_sim_player
#                miniz
#                fmt
#                bitsery
#                LLVMDebugInfoDWARF
#                LLVMDemangle
#                LLVMObject
#            PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#    else()
#        message(STATUS "IPO is NOT supported")
#    endif()
#endif()

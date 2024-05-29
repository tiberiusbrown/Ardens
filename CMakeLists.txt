cmake_minimum_required(VERSION 3.0)

# This logic needs to be considered before project()
set(_change_MSVC_flags FALSE)
if(WIN32)
  if(CMAKE_VERSION VERSION_LESS 3.15.0)
    set(_change_MSVC_flags TRUE)
  else()
    # Set MSVC runtime to MultiThreaded (/MT)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

project(MyProj ....)

if(_change_MSVC_flags)
  # Modify compile flags to change MSVC runtime from /MD to /MT
  set(_re_match "([\\/\\-]M)D")
  set(_re_replace "\\1T")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  string(REGEX REPLACE ${_re_match} ${_re_replace}
    CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Ardens)

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
        $<$<NOT:$<CONFIG:DEBUG>>:/Ot>
        $<$<NOT:$<CONFIG:DEBUG>>:/Ox>
    )
    add_compile_definitions(
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()

if(MSYS)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

option(ARDENS_LLVM     "Build and link LLVM (enables loading ELF and debug info)" ON)
option(ARDENS_DEBUGGER "Build debugger target" ON)
option(ARDENS_LIB      "Build static library for emulator core" OFF)
option(ARDENS_PLAYER   "Build player target" ON)
option(ARDENS_LIBRETRO "Build libretro core target" OFF)
option(ARDENS_DIST     "Build distributable game from dist/game.arduboy" OFF)

set(SDL_DEFAULT OFF)
if(NOT EMSCRIPTEN AND NOT APPLE)
    set(ARDENS_SDL_DEFAULT ON)
endif()
option(ARDENS_SDL      "Build Ardens with SDL backend" ${ARDENS_SDL_DEFAULT})

if(ARDENS_LIBRETRO AND NOT LIBRETRO_STATIC)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOS TRUE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GetGitRevisionDescription)
git_describe(ARDENS_VERSION)
message(STATUS "ARDENS_VERSION: ${ARDENS_VERSION}")
string(REGEX REPLACE "^v" "" ARDENS_VERSION_SHORT ${ARDENS_VERSION})
string(REGEX REPLACE "-.*" "" ARDENS_VERSION_SHORT ${ARDENS_VERSION_SHORT})
message(STATUS "ARDENS_VERSION_SHORT: ${ARDENS_VERSION_SHORT}")

if(EMSCRIPTEN)
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

add_subdirectory(deps/fmt)
add_subdirectory(deps/miniz)
add_subdirectory(deps/bitsery)

if(ARDENS_SDL AND (ARDENS_PLAYER OR ARDENS_DEBUGGER))
    add_subdirectory(deps/SDL)
endif()

if(ARDENS_LLVM)
	
	# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
		cmake_policy(SET CMP0135 NEW)
	endif()

    include(FetchContent)

	message(STATUS "Fetching llvm-project")
	FetchContent_Declare(
		llvm-project
		URL https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.0/llvm-project-16.0.0.src.tar.xz
		)

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
    set(LLVM_ENABLE_ZSTD                   OFF CACHE BOOL   "" FORCE)
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
	
	if(NOT llvm-project_POPULATED)
		FetchContent_Populate(llvm-project)
		add_subdirectory(${llvm-project_SOURCE_DIR}/llvm ${llvm-project_BINARY_DIR} EXCLUDE_FROM_ALL)
	endif()
endif()

set(EXEFLAG)
if(MSVC)
    set(EXEFLAG WIN32)
endif()
set(IMGUI_SOURCES
    deps/imgui/imconfig.h
    deps/imgui/imgui_internal.h
    deps/imgui/imgui.h
    deps/imgui/imgui.cpp
    #deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imstb_rectpack.h
    deps/imgui/imstb_textedit.h
    deps/imgui/imstb_truetype.h
    deps/imgui/imgui_memory_editor.h
    )
if(ARDENS_SDL)
    list(APPEND IMGUI_SOURCES
        deps/imgui/imgui_impl_sdl3.h
        deps/imgui/imgui_impl_sdl3.cpp
        deps/imgui/imgui_impl_sdlrenderer3.h
        deps/imgui/imgui_impl_sdlrenderer3.cpp
        )
endif()
set(IMPLOT_SOURCES
    deps/implot/implot.h
    deps/implot/implot.cpp
    deps/implot/implot_internal.h
    #deps/implot/implot_demo.cpp
    deps/implot/implot_items.cpp
    )
set(GIFENC_SOURCES
    deps/gifenc/gifenc.h
    deps/gifenc/gifenc.c
    )
set(SOKOL_SOURCES
    deps/sokol/sokol_app.h
    deps/sokol/sokol_args.h
    deps/sokol/sokol_audio.h
    deps/sokol/sokol_fetch.h
    deps/sokol/sokol_gfx.h
    deps/sokol/sokol_glue.h
    deps/sokol/sokol_imgui.h
    deps/sokol/sokol_time.h
    )
set(SIMPLE_FFT_SOURCES
    deps/Simple-FFT/include/simple_fft/check_fft.hpp
    deps/Simple-FFT/include/simple_fft/copy_array.hpp
    deps/Simple-FFT/include/simple_fft/error_handling.hpp
    deps/Simple-FFT/include/simple_fft/fft.h
    deps/Simple-FFT/include/simple_fft/fft.hpp
    deps/Simple-FFT/include/simple_fft/fft_impl.hpp
    deps/Simple-FFT/include/simple_fft/fft_settings.h
    )
set(HQX_SOURCES
    deps/hqx/include/hqx/HQx.hh
    deps/hqx/include/hqx/HQ2x.hh
    deps/hqx/include/hqx/HQ3x.hh
    deps/hqx/source/HQx.cc
    deps/hqx/source/HQ2x.cc
    deps/hqx/source/HQ3x.cc
    )
set(ARDENS_SOURCES
    src/absim.hpp
    src/absim_instructions.hpp
    src/absim_cpu_data.cpp

    src/absim_arduboy.cpp
    src/absim_display.hpp
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
    set(EM_FILE_BROWSER_SOURCES deps/emscripten-browser-file/emscripten_browser_file.h)
endif()

set(SYSTEM_LIBS)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(SYSTEM_LIBS X11 Xi Xcursor GL asound dl m pthread)
endif()

macro(configure_app_target target)
    if(MACOS)
        set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${target}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_PRODUCT_NAME "${target}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "${target}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${ARDENS_VERSION_SHORT}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_COPYRIGHT "Copyright Peter Brown. Released under MIT License.")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "net.odgen.${target}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_LONG_VERSION_STRING "${ARDENS_VERSION}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING "${ARDENS_VERSION_SHORT}")
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "ardens.icns")
        target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/img/ardens.icns")
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/img/ardens.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_compile_options(${target} PRIVATE -x objective-c++)
    endif()
    if(ARDENS_SDL)
        target_compile_definitions(${target} PUBLIC -DARDENS_PLATFORM_SDL)
        target_link_libraries(${target} PRIVATE SDL3-static)
    else()
        target_compile_definitions(${target} PRIVATE -DARDENS_PLATFORM_SOKOL)
    endif()
endmacro()

set(MACOS_ICON)
if(MACOS)
    set(SYSTEM_LIBS ${SYSTEM_LIBS} "-framework QuartzCore" "-framework Cocoa" "-framework AudioToolbox")
    set(SYSTEM_LIBS ${SYSTEM_LIBS} "-framework MetalKit" "-framework Metal")
    set(EXEFLAG MACOSX_BUNDLE)
    set(MACOS_ICON ${CMAKE_CURRENT_SOURCE_DIR}/img/ardens.icns)
    set_source_files_properties(${MACOS_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

set(RC_FILE)
if(WIN32)
    set(RC_FILE "img/Ardens.rc")
endif()

if(ARDENS_DEBUGGER)

    add_executable(Ardens ${EXEFLAG}
        .editorconfig

        src/main_sdl.cpp
        src/main_sokol.cpp
        ${RC_FILE}
        ${MACOS_ICON}
        src/ardens_icon.hpp

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
        src/file_watch.cpp

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

        ${ARDENS_SOURCES}

        deps/stb_image_write.h
        ${IMGUI_SOURCES}
        ${IMPLOT_SOURCES}
        ${GIFENC_SOURCES}
        ${EM_FILE_BROWSER_SOURCES}
        ${SIMPLE_FFT_SOURCES}
        ${HQX_SOURCES}
        ${SOKOL_SOURCES}

        deps/ImGuiColorTextEdit/TextEditor.h
        deps/ImGuiColorTextEdit/TextEditor.cpp
        )
    target_link_libraries(Ardens PRIVATE
        miniz
        fmt
        bitsery
        ${SYSTEM_LIBS}
        )
    if(ARDENS_LLVM)
        target_link_libraries(Ardens PRIVATE
            LLVMDebugInfoDWARF
            LLVMDemangle
            )
        target_include_directories(Ardens PRIVATE
            "$<TARGET_PROPERTY:LLVMDebugInfoDWARF,INCLUDE_DIRECTORIES>"
            "$<TARGET_PROPERTY:LLVMDemangle,INCLUDE_DIRECTORIES>"
            "$<TARGET_PROPERTY:LLVMObject,INCLUDE_DIRECTORIES>"
            )
        target_compile_definitions(Ardens PRIVATE -DARDENS_LLVM)
    endif()
    target_include_directories(Ardens PRIVATE
        deps
        deps/imgui
        deps/implot
        deps/gifenc
        deps/Simple-FFT/include
        deps/hqx/include
        deps/rapidjson/include
        )
    configure_app_target(Ardens)
        
endif()

add_library(ardenslib STATIC
    .editorconfig
    ${ARDENS_SOURCES}
    )

target_include_directories(ardenslib PUBLIC src deps deps/rapidjson/include)

target_compile_definitions(ardenslib PUBLIC -DARDENS_PLAYER)
target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_SNAPSHOTS)
target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_DEBUGGER)
target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_GUI)
target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_SAVED_SETTINGS)
target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_SCALING)
#target_compile_definitions(ardenslib PUBLIC -DARDENS_NO_ARDUBOY_FILE)

target_link_libraries(ardenslib PUBLIC bitsery miniz fmt)

if(ARDENS_PLAYER)

    add_executable(ArdensPlayer ${EXEFLAG}
        .editorconfig

        src/main_sdl.cpp
        src/main_sokol.cpp
        ${RC_FILE}
        ${MACOS_ICON}
        src/ardens_icon.hpp

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
        src/file_watch.cpp

        deps/stb_image_write.h
        ${IMGUI_SOURCES}
        ${GIFENC_SOURCES}
        ${EM_FILE_BROWSER_SOURCES}
        ${SOKOL_SOURCES}
        )
    target_include_directories(ArdensPlayer PRIVATE
        deps
        deps/imgui
        deps/gifenc
        )
    target_link_libraries(ArdensPlayer PRIVATE
        ardenslib
        ${SYSTEM_LIBS}
        )
    configure_app_target(ArdensPlayer)

endif()

add_executable(integration_tests
    .editorconfig
    tests/integration_tests.cpp
    )
target_link_libraries(integration_tests PRIVATE ardenslib)
target_compile_definitions(integration_tests PRIVATE
    -DTESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")

if(ARDENS_DIST)

    include(cmake/bin2c.cmake)

    file(GLOB ARDUBOY_FILES "dist/*.arduboy")
    foreach(ARDUBOY_FILE ${ARDUBOY_FILES})
        message(STATUS "ARDUBOY_FILE=${ARDUBOY_FILE}")
        get_filename_component(ARDENS_DIST_TARGET "${ARDUBOY_FILE}" NAME_WLE)

        set(DISTGAME_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/dist/${ARDENS_DIST_TARGET}")
        file(MAKE_DIRECTORY "${DISTGAME_OUTDIR}")

        set(DISTGAME_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ARDENS_DIST_TARGET}")
        file(MAKE_DIRECTORY "${DISTGAME_DIR}")

        bin2c(
            "${DISTGAME_DIR}/distgame.c"
            "${DISTGAME_DIR}/distgame.h"
            "${ARDUBOY_FILE}"
            "game_arduboy"
            )

        add_executable("${ARDENS_DIST_TARGET}" ${EXEFLAG}
            .editorconfig

            src/main_sdl.cpp
            src/main_sokol.cpp
            ${RC_FILE}
            ${MACOS_ICON}
            src/ardens_icon.hpp

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
            src/file_watch.cpp

            deps/stb_image_write.h
            ${IMGUI_SOURCES}
            ${GIFENC_SOURCES}
            ${EM_FILE_BROWSER_SOURCES}
            ${SOKOL_SOURCES}

            "${DISTGAME_DIR}/distgame.c"
            "${DISTGAME_DIR}/distgame.h"
            )
        target_include_directories("${ARDENS_DIST_TARGET}" PRIVATE
            deps
            deps/imgui
            deps/gifenc
            )
        target_link_libraries("${ARDENS_DIST_TARGET}" PRIVATE
            ardenslib
            ${SYSTEM_LIBS}
            )
        configure_app_target("${ARDENS_DIST_TARGET}")
        target_compile_definitions("${ARDENS_DIST_TARGET}" PRIVATE -DARDENS_DIST)
        target_compile_definitions("${ARDENS_DIST_TARGET}" PRIVATE -DARDENS_DISTNAME="${ARDENS_DIST_TARGET}")
        target_include_directories("${ARDENS_DIST_TARGET}" PRIVATE "${DISTGAME_DIR}")
        if(MSVC)
            target_compile_options("${ARDENS_DIST_TARGET}" PRIVATE /Zc:__cplusplus)
        else()
            set_target_properties("${ARDENS_DIST_TARGET}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${DISTGAME_OUTDIR}")
        endif()
    endforeach()

endif()

if(ARDENS_LIBRETRO)

    set(ARDENS_LIBRETRO_TYPE SHARED)
    if(LIBRETRO_STATIC)
        set(ARDENS_LIBRETRO_TYPE STATIC)
    endif()
    add_library(ardens_libretro ${ARDENS_LIBRETRO_TYPE}
        .editorconfig
        ${ARDENS_SOURCES}
        src/libretro_core/libretro.h
        src/libretro_core/libretro_impl.cpp
        deps/yyjson/yyjson.h
        deps/yyjson/yyjson.c
        )
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_PLAYER)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_DEBUGGER)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_GUI)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_SAVED_SETTINGS)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_SCALING)
    target_link_libraries(ardens_libretro PRIVATE
        miniz
        bitsery
        )
    target_include_directories(ardens_libretro PUBLIC src deps/yyjson)
    target_include_directories(ardens_libretro PRIVATE deps/rapidjson/include)
    # WHY IS THIS NECESSARY???
    if(MSVC AND (MSVC_VERSION GREATER_EQUAL 1914))
        target_compile_options(ardens_libretro PRIVATE "/Zc:__cplusplus")
    endif()
    if(ANDROID)
        set_target_properties(ardens_libretro PROPERTIES SUFFIX "_android.so")
    elseif(EMSCRIPTEN)
        set_target_properties(ardens_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}.bc")
    elseif(LIBRETRO_STATIC)
        set_target_properties(ardens_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}.a")
    endif()
    set_target_properties(ardens_libretro PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(ardens_libretro PROPERTIES PREFIX "")

endif()

set_source_files_properties(
    src/absim_snapshot.cpp
    src/view_debugger.cpp
    src/common.cpp
    src/libretro_core/libretro_impl.cpp
    PROPERTIES
        COMPILE_OPTIONS "-DARDENS_VERSION=\"${ARDENS_VERSION}\""
    )

if(MSVC)
    if(ARDENS_DEBUGGER)
        target_compile_options(Ardens PRIVATE /Zc:__cplusplus)
    endif()
    if(ARDENS_PLAYER OR ARDENS_DIST OR ARDENS_LIB)
        target_compile_options(ardenslib PUBLIC /Zc:__cplusplus)
    endif()
    if(ARDENS_PLAYER)
        target_compile_options(ArdensPlayer PRIVATE /Zc:__cplusplus)
    endif()
    if(ARDENS_LIBRETRO)
        target_compile_options(ardens_libretro PUBLIC /Zc:__cplusplus)
    endif()
endif()

if(EMSCRIPTEN)
    if(ARDENS_DEBUGGER)
        target_link_options(Ardens PRIVATE "-lidbfs.js")
    endif()
    if(ARDENS_PLAYER)
        target_link_options(ArdensPlayer PRIVATE "-lidbfs.js")
    endif()
endif()

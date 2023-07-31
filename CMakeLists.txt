cmake_minimum_required(VERSION 3.15)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Ardens)

include(FetchContent)

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
endif()

option(ARDENS_LLVM     "Build and link LLVM (enables loading ELF and debug info)" ON)
option(ARDENS_DEBUGGER "Build debugger target" ON)
option(ARDENS_PLAYER   "Build player target" ON)
option(ARDENS_LIBRETRO "Build libretro core target" OFF)

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

if(ARDENS_LLVM)
	
	# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
		cmake_policy(SET CMP0135 NEW)
	endif()

	message(STATUS "Fetching llvm-project")
	FetchContent_Declare(
		llvm-project
		URL https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/llvm-project-15.0.7.src.tar.xz
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
    set(LLVM_ENABLE_ZSTD                   ON  CACHE BOOL   "" FORCE)
    set(LLVM_USE_STATIC_ZSTD               ON  CACHE BOOL   "" FORCE)
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
    src/imgui/imconfig.h
    src/imgui/imgui_internal.h
    src/imgui/imgui.h
    src/imgui/imgui.cpp
    #src/imgui/imgui_demo.cpp
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
set(ARDENS_SOURCES
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

set(SYSTEM_LIBS)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(SYSTEM_LIBS X11 Xi Xcursor GL asound dl m)
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
        set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE  "ardens.icns")
        target_compile_options(${target} PRIVATE -x objective-c++)
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
    target_compile_definitions(Ardens PRIVATE -DARDENS_PLATFORM_SOKOL)
    target_include_directories(Ardens PRIVATE
        src/imgui
        src/implot
        src/gifenc
        src/Simple-FFT/include
        src/hqx/include
        src/rapidjson/include
        )
    configure_app_target(Ardens)
        
endif()

if(ARDENS_PLAYER)

    add_executable(ArdensPlayer ${EXEFLAG}
        .editorconfig

        src/main_sdl.cpp
        src/main_sokol.cpp
        ${RC_FILE}
        ${MACOS_ICON}

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

        ${ARDENS_SOURCES}

        src/stb_image_write.h
        ${IMGUI_SOURCES}
        ${GIFENC_SOURCES}
        ${EM_FILE_BROWSER_SOURCES}
        ${SOKOL_SOURCES}
        )
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_PLAYER)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_SNAPSHOTS)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_DEBUGGER)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_GUI)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_SAVED_SETTINGS)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_SCALING)
    #target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_NO_ARDUBOY_FILE)
    target_compile_definitions(ArdensPlayer PRIVATE -DARDENS_PLATFORM_SOKOL)
    target_include_directories(ArdensPlayer PRIVATE
        src
        src/imgui
        src/gifenc
        src/rapidjson/include
        )
    target_link_libraries(ArdensPlayer PRIVATE
        miniz
        fmt
        bitsery
        ${SYSTEM_LIBS}
        )
    configure_app_target(ArdensPlayer)

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
        )
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_PLAYER)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_DEBUGGER)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_GUI)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_SAVED_SETTINGS)
    target_compile_definitions(ardens_libretro PRIVATE -DARDENS_NO_SCALING)
    target_link_libraries(ardens_libretro PRIVATE
        miniz
        fmt
        bitsery
        )
    target_include_directories(ardens_libretro PUBLIC src)
    target_include_directories(ardens_libretro PRIVATE src/rapidjson/include)
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
    if(ARDENS_PLAYER)
        target_compile_options(ArdensPlayer PRIVATE /Zc:__cplusplus)
    endif()
    if(ARDENS_LIBRETRO)
        target_compile_options(ardens_libretro PRIVATE /Zc:__cplusplus)
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

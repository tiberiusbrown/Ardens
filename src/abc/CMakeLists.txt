cmake_minimum_required(VERSION 3.15)

project(ardscript)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus /utf-8 /D_CRT_SECURE_NO_DEPRECATE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

add_library(ards_assembler STATIC
    .editorconfig
    ards_vm/ards_instr.hpp
    ards_assembler.cpp
    ards_assembler.hpp
    ards_error.hpp
    )
target_include_directories(ards_assembler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(ards_compiler STATIC
    .editorconfig
    ards_vm/ards_instr.hpp
    ards_assembler.hpp
    ards_assembler.cpp
    ards_compiler.cpp
    ards_compiler.hpp
    ards_compiler_codegen.cpp
    ards_compiler_parse.cpp
    ards_compiler_peephole.cpp
    ards_compiler_transforms.cpp
    ards_compiler_type.cpp
    ards_compiler_write.cpp
    ards_error.hpp
    deps/peglib/peglib.h
    )
target_include_directories(ards_compiler PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/peglib"
    )

add_executable(ards_as ards_as.cpp)
target_link_libraries(ards_as ards_assembler)

add_executable(ards_c ards_c.cpp)
target_link_libraries(ards_c ards_compiler)

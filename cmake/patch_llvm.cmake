cmake_minimum_required(VERSION 3.5)

if(NOT GIT_EXECUTABLE)
    find_package(Git REQUIRED)
endif()

set(llvm_patch "${CMAKE_CURRENT_LIST_DIR}/llvm-16-smallvector-cstdint.patch")
# Keep Git from discovering Ardens when the build directory is in its worktree.
get_filename_component(llvm_source "${LLVM_SOURCE_DIR}" REALPATH)
get_filename_component(llvm_source_parent "${llvm_source}" DIRECTORY)
set(git_apply "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${llvm_source_parent}"
    "${GIT_EXECUTABLE}" -c core.autocrlf=input apply)

# FetchContent may run the patch step again against a cached source tree.
execute_process(
    COMMAND ${git_apply} --reverse --check "${llvm_patch}"
    WORKING_DIRECTORY "${llvm_source}"
    RESULT_VARIABLE already_applied
    ERROR_QUIET
)
if(NOT already_applied STREQUAL "0")
    execute_process(
        COMMAND ${git_apply} "${llvm_patch}"
        WORKING_DIRECTORY "${llvm_source}"
        RESULT_VARIABLE patch_result
    )
    if(NOT patch_result STREQUAL "0")
        message(FATAL_ERROR "Failed to apply ${llvm_patch} in ${llvm_source}: ${patch_result}")
    endif()
endif()

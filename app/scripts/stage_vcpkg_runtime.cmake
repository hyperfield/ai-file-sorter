if(NOT DEFINED AIFS_VCPKG_INSTALLED_ROOT OR AIFS_VCPKG_INSTALLED_ROOT STREQUAL "")
    message(FATAL_ERROR "AIFS_VCPKG_INSTALLED_ROOT is required")
endif()

if(NOT DEFINED AIFS_VCPKG_TARGET_TRIPLET OR AIFS_VCPKG_TARGET_TRIPLET STREQUAL "")
    message(FATAL_ERROR "AIFS_VCPKG_TARGET_TRIPLET is required")
endif()

if(NOT DEFINED AIFS_DEST_DIR OR AIFS_DEST_DIR STREQUAL "")
    message(FATAL_ERROR "AIFS_DEST_DIR is required")
endif()

set(_aifs_runtime_dir
    "${AIFS_VCPKG_INSTALLED_ROOT}/${AIFS_VCPKG_TARGET_TRIPLET}/bin")

if(DEFINED AIFS_TARGET_CONFIG AND AIFS_TARGET_CONFIG STREQUAL "Debug")
    set(_aifs_runtime_dir
        "${AIFS_VCPKG_INSTALLED_ROOT}/${AIFS_VCPKG_TARGET_TRIPLET}/debug/bin")
endif()

if(NOT EXISTS "${_aifs_runtime_dir}")
    message(STATUS "Skipping vcpkg runtime staging, directory not found: ${_aifs_runtime_dir}")
    return()
endif()

file(GLOB _aifs_runtime_dlls "${_aifs_runtime_dir}/*.dll")
list(REMOVE_DUPLICATES _aifs_runtime_dlls)

if(NOT _aifs_runtime_dlls)
    return()
endif()

file(MAKE_DIRECTORY "${AIFS_DEST_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${_aifs_runtime_dlls} "${AIFS_DEST_DIR}"
    RESULT_VARIABLE _aifs_copy_result)

if(NOT _aifs_copy_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to stage vcpkg runtime DLLs from ${_aifs_runtime_dir} to ${AIFS_DEST_DIR}")
endif()

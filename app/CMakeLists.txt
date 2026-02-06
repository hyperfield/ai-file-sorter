cmake_minimum_required(VERSION 3.21)

project(AIFileSorter LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AI_FILE_SORTER_BUILD_TESTS "Build unit tests (requires Catch2 submodule)" OFF)

# Prefer MSVC on Windows if available
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Third-party deps (resolved best via vcpkg on Windows)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
# JsonCpp may not ship a CMake config on some distros (e.g. Ubuntu). Try
# config mode first, then fall back to pkg-config.
find_package(JsonCpp CONFIG QUIET)
if(NOT JsonCpp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    add_library(JsonCpp::JsonCpp INTERFACE IMPORTED)
    target_include_directories(JsonCpp::JsonCpp INTERFACE ${JSONCPP_INCLUDE_DIRS})
    target_link_directories(JsonCpp::JsonCpp INTERFACE ${JSONCPP_LIBRARY_DIRS})
    target_link_libraries(JsonCpp::JsonCpp INTERFACE ${JSONCPP_LINK_LIBRARIES})
else()
    if(NOT TARGET JsonCpp::JsonCpp)
        add_library(JsonCpp::JsonCpp INTERFACE IMPORTED)
    endif()
endif()
find_package(fmt REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(Intl REQUIRED) # libintl/gettext

# Sources
set(APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB APP_LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
)
set(APP_SOURCES ${APP_MAIN_SOURCE} ${APP_LIB_SOURCES})

# Executable (GUI subsystem on Windows)
if(WIN32)
    add_executable(aifilesorter WIN32 ${APP_SOURCES})
else()
    add_executable(aifilesorter ${APP_SOURCES})
endif()

target_include_directories(aifilesorter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
)

# Resources (equivalent to rcc generation in Makefile)
set(APP_RESOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/.env"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/logo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/qn_logo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/app_icon_128.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/icon_512x512.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/certs/cacert.pem"
)

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/.env"
    PROPERTIES QT_RESOURCE_ALIAS ".env"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/logo.png"
    PROPERTIES QT_RESOURCE_ALIAS "images/logo.png"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/qn_logo.png"
    PROPERTIES QT_RESOURCE_ALIAS "images/qn_logo.png"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/app_icon_128.png"
    PROPERTIES QT_RESOURCE_ALIAS "images/app_icon_128.png"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/icon_512x512.png"
    PROPERTIES QT_RESOURCE_ALIAS "images/icon_512x512.png"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/certs/cacert.pem"
    PROPERTIES QT_RESOURCE_ALIAS "certs/cacert.pem"
)

qt_add_resources(aifilesorter "app_resources"
    PREFIX "/net/quicknode/AIFileSorter"
    FILES ${APP_RESOURCE_FILES}
)

if(WIN32)
    if(DEFINED ENV{WindowsSdkDir} AND DEFINED ENV{WindowsSDKLibVersion})
        set(WIN_SDK_UM_DIR "$ENV{WindowsSdkDir}Lib/$ENV{WindowsSDKLibVersion}/um/x64")
        set(WIN_SDK_UCRT_DIR "$ENV{WindowsSdkDir}Lib/$ENV{WindowsSDKLibVersion}/ucrt/x64")
        if(EXISTS "${WIN_SDK_UM_DIR}")
            link_directories("${WIN_SDK_UM_DIR}")
        endif()
        if(EXISTS "${WIN_SDK_UCRT_DIR}")
            link_directories("${WIN_SDK_UCRT_DIR}")
        endif()
    endif()

    set(PRECOMPILED_CPU_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cpu/lib")
    set(PRECOMPILED_CPU_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cpu/bin")
    set(LLAMA_CPU_IMPORT "${PRECOMPILED_CPU_LIB_DIR}/llama.lib")
    set(LLAMA_CPU_DLL "${PRECOMPILED_CPU_BIN_DIR}/llama.dll")
    if(NOT EXISTS "${LLAMA_CPU_IMPORT}")
        message(FATAL_ERROR "Missing ${LLAMA_CPU_IMPORT}. Run app/scripts/build_llama_windows.ps1 cuda=off vcpkgroot=<path> first.")
    endif()
    if(NOT EXISTS "${LLAMA_CPU_DLL}")
        message(FATAL_ERROR "Missing ${LLAMA_CPU_DLL}. Run app/scripts/build_llama_windows.ps1 cuda=off vcpkgroot=<path> to stage runtime DLLs.")
    endif()
    add_library(llama SHARED IMPORTED)
    set_target_properties(llama PROPERTIES
        IMPORTED_IMPLIB "${LLAMA_CPU_IMPORT}"
        IMPORTED_LOCATION "${LLAMA_CPU_DLL}"
    )

    foreach(libName IN ITEMS ggml ggml-base ggml-cpu)
        set(import_path "${PRECOMPILED_CPU_LIB_DIR}/${libName}.lib")
        set(dll_path "${PRECOMPILED_CPU_BIN_DIR}/${libName}.dll")
        if(NOT EXISTS "${import_path}")
            message(FATAL_ERROR "Missing ${import_path}. Run app/scripts/build_llama_windows.ps1 cuda=off vcpkgroot=<path> first.")
        endif()
        if(NOT EXISTS "${dll_path}")
            message(FATAL_ERROR "Missing ${dll_path}. Run app/scripts/build_llama_windows.ps1 cuda=off vcpkgroot=<path> to stage runtime DLLs.")
        endif()
        string(REPLACE "-" "_" imported_target "${libName}")
        add_library(${imported_target} SHARED IMPORTED)
        set_target_properties(${imported_target} PROPERTIES
            IMPORTED_IMPLIB "${import_path}"
            IMPORTED_LOCATION "${dll_path}"
        )
    endforeach()
else()
    # Build llama.cpp from the included submodule for non-Windows platforms
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp" "${CMAKE_CURRENT_BINARY_DIR}/llama-build")
    target_link_libraries(aifilesorter PRIVATE llama)
    if(MSVC)
        target_compile_options(llama PRIVATE /Zc:char8_t-)
    endif()
endif()

# Link libraries
target_link_libraries(aifilesorter PRIVATE
    Qt6::Widgets Qt6::Gui Qt6::Core
    CURL::libcurl
    OpenSSL::SSL OpenSSL::Crypto
    SQLite::SQLite3
    JsonCpp::JsonCpp
    fmt::fmt
    spdlog::spdlog
    Intl::Intl
    llama           # imported on Windows or built from submodule on other platforms
)

if(WIN32)
    target_link_libraries(aifilesorter PRIVATE ggml ggml_base ggml_cpu)
endif()

if(WIN32)
    set(STARTER_TARGET StartAiFileSorter)
    add_executable(${STARTER_TARGET} WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/startapp_windows.cpp"
    )
    target_link_libraries(${STARTER_TARGET} PRIVATE
        Qt6::Widgets Qt6::Gui Qt6::Core
    )
    target_compile_definitions(${STARTER_TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# On Windows, ensure Unicode and lean Windows headers in deps that need it
if(WIN32)
    target_compile_definitions(aifilesorter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set(WIN_SYSTEM_LIBS wininet d3d11 dxgi dxguid d3d12 mpr userenv)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_win_sdk_arch "x64")
    else()
        set(_win_sdk_arch "x86")
    endif()

    set(_win_sdk_lib_paths "")
    if(DEFINED ENV{WindowsSdkDir} AND DEFINED ENV{WindowsSDKLibVersion})
        file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" _sdk_dir)
        file(TO_CMAKE_PATH "$ENV{WindowsSDKLibVersion}" _sdk_version_raw)
        string(REGEX REPLACE "/$" "" _sdk_version "${_sdk_version_raw}")
        set(_sdk_root "${_sdk_dir}Lib/${_sdk_version}")
        foreach(_subdir IN ITEMS um ucrt)
            set(_candidate "${_sdk_root}/${_subdir}/${_win_sdk_arch}")
            if(EXISTS "${_candidate}")
                list(APPEND _win_sdk_lib_paths "${_candidate}")
            endif()
        endforeach()
    endif()

    if(NOT _win_sdk_lib_paths AND DEFINED CMAKE_RC_COMPILER AND CMAKE_RC_COMPILER)
        get_filename_component(_rc_dir "${CMAKE_RC_COMPILER}" DIRECTORY)
        get_filename_component(_bin_version_dir "${_rc_dir}" DIRECTORY)
        get_filename_component(_bin_dir "${_bin_version_dir}" DIRECTORY)
        get_filename_component(_kits_root "${_bin_dir}" DIRECTORY)
        get_filename_component(_sdk_version "${_bin_version_dir}" NAME)
        if(_kits_root AND EXISTS "${_kits_root}/Lib/${_sdk_version}")
            set(_sdk_root "${_kits_root}/Lib/${_sdk_version}")
            foreach(_subdir IN ITEMS um ucrt)
                set(_candidate "${_sdk_root}/${_subdir}/${_win_sdk_arch}")
                if(EXISTS "${_candidate}")
                    list(APPEND _win_sdk_lib_paths "${_candidate}")
                endif()
            endforeach()
        endif()
    endif()

    foreach(libName IN LISTS WIN_SYSTEM_LIBS)
        string(TOUPPER "${libName}" upperLib)
        set(varName "${upperLib}_LIBRARY")
        unset(${varName})
        if(_win_sdk_lib_paths)
            find_library(${varName} NAMES ${libName}
                PATHS ${_win_sdk_lib_paths}
                NO_DEFAULT_PATH
            )
        endif()
        if(NOT ${varName})
            find_library(${varName} NAMES ${libName})
        endif()

        # --- FIX START: Fallback to name-only linking if file isn't found ---
        if(NOT ${varName})
            message(STATUS "System library '${libName}' not found on disk. Configuring to link by name.")
            set(${varName} ${libName})
            # If this is inside a function, we must export the variable to the parent scope
            set(${varName} ${libName} PARENT_SCOPE)
        endif()
        # --- FIX END ---
        target_link_libraries(aifilesorter PRIVATE "${${varName}}")
    endforeach()
endif()

if(WIN32)
    find_program(POWERSHELL_EXECUTABLE NAMES pwsh pwsh.exe powershell powershell.exe)
    if(NOT POWERSHELL_EXECUTABLE)
        message(FATAL_ERROR "PowerShell is required to generate Windows resources")
    endif()

    set(APP_ICON_BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/icon_256x256.png")
    set(APP_ICON_ICO "${CMAKE_CURRENT_BINARY_DIR}/generated/app_icon.ico")
    add_custom_command(OUTPUT "${APP_ICON_ICO}"
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_icon.ps1" -BasePng "${APP_ICON_BASE}" -Ico "${APP_ICON_ICO}"
        DEPENDS "${APP_ICON_BASE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_icon.ps1"
        COMMENT "Generating Windows icon"
        VERBATIM
    )
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/generated/app_icon.rc")
    file(TO_NATIVE_PATH "${APP_ICON_ICO}" APP_ICON_ICO_NATIVE_TEMP)
    string(REPLACE "\\" "\\\\" APP_ICON_ICO_NATIVE "${APP_ICON_ICO_NATIVE_TEMP}")
    set(APP_ICON_PATH "${APP_ICON_ICO_NATIVE}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/app_icon.rc.in"
        "${APP_ICON_RC}"
        @ONLY
    )
    set_source_files_properties("${APP_ICON_ICO}" PROPERTIES GENERATED TRUE)
    add_custom_target(app_icon_resource ALL DEPENDS "${APP_ICON_ICO}")
    add_dependencies(aifilesorter app_icon_resource)
    add_dependencies(aifilesorter app_icon_resource)
    if(WIN32)
        add_dependencies(${STARTER_TARGET} app_icon_resource)
    endif()
    target_sources(aifilesorter PRIVATE "${APP_ICON_RC}")
    target_sources(aifilesorter PRIVATE "${APP_ICON_RC}")
    if(WIN32)
        target_sources(${STARTER_TARGET} PRIVATE "${APP_ICON_RC}")
    endif()

    # Version info resource generated from app_version.hpp
    set(APP_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/app_version.hpp")
    file(READ "${APP_VERSION_HEADER}" APP_VERSION_CONTENTS)
    string(REGEX MATCH "Version\\{([0-9]+),[ \t]*([0-9]+),[ \t]*([0-9]+)" _ "${APP_VERSION_CONTENTS}")
    if(CMAKE_MATCH_1)
        set(APP_VER_MAJOR "${CMAKE_MATCH_1}")
        set(APP_VER_MINOR "${CMAKE_MATCH_2}")
        set(APP_VER_PATCH "${CMAKE_MATCH_3}")
    else()
        set(APP_VER_MAJOR "0")
        set(APP_VER_MINOR "0")
        set(APP_VER_PATCH "0")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
        @ONLY
    )
    target_sources(aifilesorter PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc")
    target_sources(${STARTER_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc")

    set(PRECOMPILED_CPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cpu")
    set(PRECOMPILED_CUDA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/cuda")
    set(PRECOMPILED_VULKAN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/precompiled/vulkan")

    add_custom_command(TARGET aifilesorter POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E remove -f "$<TARGET_FILE_DIR:aifilesorter>/openblas.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wocuda"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wcuda"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wvulkan"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PRECOMPILED_CPU_ROOT}"
            "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wocuda"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PRECOMPILED_CUDA_ROOT}"
            "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wcuda"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PRECOMPILED_VULKAN_ROOT}"
            "$<TARGET_FILE_DIR:aifilesorter>/lib/ggml/wvulkan"
    )

    # set(_precompiled_libopenblas "${PRECOMPILED_CPU_ROOT}/bin/libopenblas.dll")
    # if(EXISTS "${_precompiled_libopenblas}")
    #     add_custom_command(TARGET aifilesorter POST_BUILD
    #         COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #             "${_precompiled_libopenblas}"
    #             "$<TARGET_FILE_DIR:aifilesorter>/libopenblas.dll"
    #     )
    # else()
    #     message(WARNING "Expected ${_precompiled_libopenblas} to exist. Run app/scripts/build_llama_windows.ps1 to stage GGML artifacts.")
    # endif()
endif()

if(AI_FILE_SORTER_BUILD_TESTS)
    include(CTest)
    set(_catch2_source_dir "${CMAKE_SOURCE_DIR}/../external/Catch2")
    if(EXISTS "${_catch2_source_dir}/CMakeLists.txt")
        add_subdirectory(${_catch2_source_dir} ${CMAKE_BINARY_DIR}/catch2-build)
    else()
        message(STATUS "Catch2 submodule not found, fetching via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(ai_file_sorter_tests
        ${APP_LIB_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_utils.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_file_scanner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_local_llm_backend.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llm_downloader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_translation.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_ui_translator.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_categorization_dialog.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_support_prompt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_whitelist_and_prompt.cpp"
    )

    target_include_directories(ai_file_sorter_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit"
    )

    target_link_libraries(ai_file_sorter_tests PRIVATE
        Catch2::Catch2WithMain
        Qt6::Core
        Qt6::Widgets
        CURL::libcurl
        OpenSSL::SSL OpenSSL::Crypto
        SQLite::SQLite3
        JsonCpp::JsonCpp
        spdlog::spdlog
        fmt::fmt
        Intl::Intl
        llama
    )

    target_compile_definitions(ai_file_sorter_tests PRIVATE AI_FILE_SORTER_TEST_BUILD=1 WIN32_LEAN_AND_MEAN NOMINMAX)

    if(WIN32)
        target_link_libraries(ai_file_sorter_tests PRIVATE ggml ggml_base ggml_cpu)
        foreach(libName IN LISTS WIN_SYSTEM_LIBS)
            string(TOUPPER "${libName}" upperLib)
            set(varName "${upperLib}_LIBRARY")
            if(DEFINED ${varName} AND ${varName})
                target_link_libraries(ai_file_sorter_tests PRIVATE "${${varName}}")
            else()
                target_link_libraries(ai_file_sorter_tests PRIVATE ${libName})
            endif()
        endforeach()
    endif()

    if(BUILD_TESTING)
        add_test(NAME ai_file_sorter_tests COMMAND ai_file_sorter_tests)
    endif()

    if(NOT WIN32)
        add_test(
            NAME integration_run_all
            COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_SOURCE_DIR}/../tests/run_all_tests.sh
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
        )
        set_tests_properties(integration_run_all PROPERTIES
            PASS_REGULAR_EXPRESSION "All tests completed successfully."
        )
    endif()
endif()

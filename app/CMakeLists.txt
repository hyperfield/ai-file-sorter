cmake_minimum_required(VERSION 3.21)

project(AIFileSorter LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AI_FILE_SORTER_BUILD_TESTS "Build unit tests (requires Catch2 submodule)" OFF)
option(AI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND
    "Require vendored PDFium for PDF extraction instead of silently falling back to external CLI tools." ON)
set(AI_FILE_SORTER_UPDATE_MODE "PLATFORM_DEFAULT" CACHE STRING
    "Updater behavior for this build: PLATFORM_DEFAULT, AUTO_INSTALL, NOTIFY_ONLY, or DISABLED")
set_property(CACHE AI_FILE_SORTER_UPDATE_MODE PROPERTY STRINGS
    PLATFORM_DEFAULT AUTO_INSTALL NOTIFY_ONLY DISABLED)

set(_aifs_effective_update_mode "${AI_FILE_SORTER_UPDATE_MODE}")
if(_aifs_effective_update_mode STREQUAL "PLATFORM_DEFAULT")
    if(WIN32)
        set(_aifs_effective_update_mode "AUTO_INSTALL")
    else()
        set(_aifs_effective_update_mode "NOTIFY_ONLY")
    endif()
endif()

set(_aifs_allowed_update_modes AUTO_INSTALL NOTIFY_ONLY DISABLED)
list(FIND _aifs_allowed_update_modes "${_aifs_effective_update_mode}" _aifs_update_mode_index)
if(_aifs_update_mode_index EQUAL -1)
    message(FATAL_ERROR
        "Unsupported AI_FILE_SORTER_UPDATE_MODE='${AI_FILE_SORTER_UPDATE_MODE}'. "
        "Expected PLATFORM_DEFAULT, AUTO_INSTALL, NOTIFY_ONLY, or DISABLED.")
endif()

if(_aifs_effective_update_mode STREQUAL "AUTO_INSTALL")
    set(AI_FILE_SORTER_UPDATE_MODE_DEFINE AI_FILE_SORTER_UPDATE_MODE_AUTO_INSTALL)
elseif(_aifs_effective_update_mode STREQUAL "NOTIFY_ONLY")
    set(AI_FILE_SORTER_UPDATE_MODE_DEFINE AI_FILE_SORTER_UPDATE_MODE_NOTIFY_ONLY)
else()
    set(AI_FILE_SORTER_UPDATE_MODE_DEFINE AI_FILE_SORTER_UPDATE_MODE_DISABLED)
endif()

if(DEFINED VCPKG_INSTALLED_DIR AND NOT "${VCPKG_INSTALLED_DIR}" STREQUAL "")
    set(AIFS_VCPKG_INSTALLED_ROOT "${VCPKG_INSTALLED_DIR}")
else()
    set(AIFS_VCPKG_INSTALLED_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed")
endif()

message(STATUS "AI File Sorter updater mode: ${_aifs_effective_update_mode}")

include(CheckSymbolExists)
include(CheckCXXSourceCompiles)

function(aifs_apply_update_mode target_name)
    target_compile_definitions(${target_name} PRIVATE ${AI_FILE_SORTER_UPDATE_MODE_DEFINE})
endfunction()

function(aifs_apply_named_update_mode target_name update_mode)
    if(update_mode STREQUAL "AUTO_INSTALL")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_UPDATE_MODE_AUTO_INSTALL)
    elseif(update_mode STREQUAL "NOTIFY_ONLY")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_UPDATE_MODE_NOTIFY_ONLY)
    elseif(update_mode STREQUAL "DISABLED")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_UPDATE_MODE_DISABLED)
    else()
        message(FATAL_ERROR
            "Unsupported explicit updater mode '${update_mode}' for target '${target_name}'.")
    endif()
endfunction()

function(aifs_apply_expected_update_mode target_name update_mode)
    if(update_mode STREQUAL "AUTO_INSTALL")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_EXPECTED_UPDATE_MODE_AUTO_INSTALL)
    elseif(update_mode STREQUAL "NOTIFY_ONLY")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_EXPECTED_UPDATE_MODE_NOTIFY_ONLY)
    elseif(update_mode STREQUAL "DISABLED")
        target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_EXPECTED_UPDATE_MODE_DISABLED)
    else()
        message(FATAL_ERROR
            "Unsupported expected updater mode '${update_mode}' for target '${target_name}'.")
    endif()
endfunction()

# Prefer MSVC on Windows if available
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Keep MSVC worker processes busy within each project in addition to
    # the top-level cmake --build --parallel job scheduling.
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:/MP>"
        "$<$<COMPILE_LANGUAGE:CXX>:/MP>"
    )
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6LinguistTools CONFIG QUIET)
if(NOT Qt6LinguistTools_FOUND)
    set(_aifs_qttools_hints "")
    if(DEFINED ENV{HOMEBREW_PREFIX} AND NOT "$ENV{HOMEBREW_PREFIX}" STREQUAL "")
        list(APPEND _aifs_qttools_hints "$ENV{HOMEBREW_PREFIX}/opt/qttools")
    endif()
    list(APPEND _aifs_qttools_hints /opt/homebrew/opt/qttools /usr/local/opt/qttools)
    if(DEFINED Qt6_DIR AND NOT "${Qt6_DIR}" STREQUAL "")
        get_filename_component(_aifs_qt6_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        list(APPEND _aifs_qttools_hints "${_aifs_qt6_prefix}")
    endif()
    list(REMOVE_DUPLICATES _aifs_qttools_hints)
    foreach(_aifs_qttools_hint IN LISTS _aifs_qttools_hints)
        if(EXISTS "${_aifs_qttools_hint}/lib/cmake/Qt6LinguistTools/Qt6LinguistToolsConfig.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_aifs_qttools_hint}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    find_package(Qt6LinguistTools CONFIG REQUIRED)
endif()

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
# MediaInfoLib from vcpkg depends on the separate ZenLib package and refers to
# its imported target as plain "zen".
find_package(ZenLib CONFIG QUIET)

# MediaInfoLib policy:
# - Must come from package managers (apt/dnf/pacman/brew/vcpkg)
# - Vendored submodules / checked-in binaries are rejected
option(AI_FILE_SORTER_REQUIRE_MEDIAINFOLIB
    "Require MediaInfoLib at configure time for full audio/video metadata extraction" ON)
option(AI_FILE_SORTER_ALLOW_VENDORED_MEDIAINFOLIB
    "Allow vendored MediaInfo in the repository (not recommended)" OFF)

file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." AI_FILE_SORTER_REPO_ROOT)

set(_aifs_blocked_mediainfo_paths
    "${AI_FILE_SORTER_REPO_ROOT}/external/MediaInfoLib"
    "${AI_FILE_SORTER_REPO_ROOT}/external/libmediainfo"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/mediainfo"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/MediaInfo"
)
if(NOT AI_FILE_SORTER_ALLOW_VENDORED_MEDIAINFOLIB)
    foreach(_blocked_path IN LISTS _aifs_blocked_mediainfo_paths)
        if(EXISTS "${_blocked_path}")
            message(FATAL_ERROR
                "Found vendored MediaInfo content at '${_blocked_path}'. "
                "Use a package-managed libmediainfo (apt/dnf/pacman/brew/vcpkg) instead.")
        endif()
    endforeach()
endif()

function(aifs_assert_path_not_in_repo path_value label)
    if(NOT path_value)
        return()
    endif()

    file(TO_CMAKE_PATH "${AI_FILE_SORTER_REPO_ROOT}" _repo_root_norm)
    string(REGEX REPLACE "([][+.*^$(){}|?\\\\])" "\\\\\\1" _repo_root_regex "${_repo_root_norm}")

    foreach(_entry IN LISTS path_value)
        if(NOT _entry OR _entry MATCHES "^\\$<")
            continue()
        endif()

        file(TO_CMAKE_PATH "${_entry}" _entry_norm)
        # vcpkg manifest mode can stage package-managed headers/libs inside the
        # build tree under a local vcpkg_installed directory. That is still a
        # package-manager origin, not vendored MediaInfo content.
        if(_entry_norm MATCHES "(^|/)vcpkg_installed(/|$)")
            continue()
        endif()
        if(_entry_norm MATCHES "^${_repo_root_regex}(/|$)")
            message(FATAL_ERROR
                "MediaInfo must come from a package manager. "
                "Repository-local path detected for ${label}: '${_entry}'.")
        endif()
    endforeach()
endfunction()

function(aifs_assert_mediainfo_target_external target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    foreach(_prop IN ITEMS IMPORTED_LOCATION IMPORTED_IMPLIB INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LINK_DIRECTORIES)
        get_target_property(_prop_value ${target_name} ${_prop})
        if(NOT _prop_value OR _prop_value STREQUAL "_prop_value-NOTFOUND")
            continue()
        endif()
        aifs_assert_path_not_in_repo("${_prop_value}" "${target_name}:${_prop}")
    endforeach()
endfunction()

set(MEDIAINFO_DEPS_LIBS "")
find_package(MediaInfoLib CONFIG QUIET)
if(TARGET MediaInfoLib::MediaInfoLib)
    list(APPEND MEDIAINFO_DEPS_LIBS MediaInfoLib::MediaInfoLib)
elseif(TARGET MediaInfoLib::MediaInfo)
    list(APPEND MEDIAINFO_DEPS_LIBS MediaInfoLib::MediaInfo)
elseif(TARGET MediaInfo::MediaInfo)
    list(APPEND MEDIAINFO_DEPS_LIBS MediaInfo::MediaInfo)
elseif(DEFINED MediaInfoLib_LIBRARY AND TARGET ${MediaInfoLib_LIBRARY})
    # The vcpkg MediaInfoLib port exports a plain imported target named
    # "mediainfo" and exposes that name through MediaInfoLib_LIBRARY.
    list(APPEND MEDIAINFO_DEPS_LIBS ${MediaInfoLib_LIBRARY})
elseif(TARGET mediainfo)
    list(APPEND MEDIAINFO_DEPS_LIBS mediainfo)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MEDIAINFO QUIET IMPORTED_TARGET libmediainfo)
        if(TARGET PkgConfig::MEDIAINFO)
            list(APPEND MEDIAINFO_DEPS_LIBS PkgConfig::MEDIAINFO)
        endif()
    endif()
    if(NOT MEDIAINFO_DEPS_LIBS AND APPLE)
        set(_aifs_mediainfo_prefixes "")
        if(DEFINED ENV{HOMEBREW_PREFIX} AND NOT "$ENV{HOMEBREW_PREFIX}" STREQUAL "")
            list(APPEND _aifs_mediainfo_prefixes "$ENV{HOMEBREW_PREFIX}")
        endif()
        list(APPEND _aifs_mediainfo_prefixes /opt/homebrew /usr/local)
        list(REMOVE_DUPLICATES _aifs_mediainfo_prefixes)

        set(_aifs_mediainfo_include_hints "")
        set(_aifs_mediainfo_library_hints "")
        foreach(_aifs_prefix IN LISTS _aifs_mediainfo_prefixes)
            list(APPEND _aifs_mediainfo_include_hints
                "${_aifs_prefix}/opt/libmediainfo/include"
                "${_aifs_prefix}/include"
            )
            list(APPEND _aifs_mediainfo_library_hints
                "${_aifs_prefix}/opt/libmediainfo/lib"
                "${_aifs_prefix}/lib"
            )
        endforeach()

        find_path(AIFS_MEDIAINFO_INCLUDE_DIR
            NAMES MediaInfo/MediaInfo.h
            HINTS ${_aifs_mediainfo_include_hints}
        )
        find_library(AIFS_MEDIAINFO_LIBRARY
            NAMES mediainfo libmediainfo
            HINTS ${_aifs_mediainfo_library_hints}
        )
        if(AIFS_MEDIAINFO_INCLUDE_DIR AND AIFS_MEDIAINFO_LIBRARY)
            if(NOT TARGET AIFS::MediaInfoLib)
                add_library(AIFS::MediaInfoLib UNKNOWN IMPORTED)
                set_target_properties(AIFS::MediaInfoLib PROPERTIES
                    IMPORTED_LOCATION "${AIFS_MEDIAINFO_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${AIFS_MEDIAINFO_INCLUDE_DIR}"
                )
            endif()
            list(APPEND MEDIAINFO_DEPS_LIBS AIFS::MediaInfoLib)
        endif()

        unset(_aifs_mediainfo_include_hints)
        unset(_aifs_mediainfo_library_hints)
        unset(_aifs_mediainfo_prefixes)
    endif()
endif()
if(MEDIAINFO_DEPS_LIBS)
    foreach(_mediainfo_target IN LISTS MEDIAINFO_DEPS_LIBS)
        aifs_assert_mediainfo_target_external(${_mediainfo_target})
    endforeach()
    add_compile_definitions(AI_FILE_SORTER_USE_MEDIAINFOLIB)
    message(STATUS "MediaInfoLib found via package-managed dependency: enabling full audio/video metadata extraction.")
elseif(AI_FILE_SORTER_REQUIRE_MEDIAINFOLIB)
    message(FATAL_ERROR
        "MediaInfoLib is required but was not found. Install libmediainfo development files "
        "(Linux: libmediainfo-dev, macOS: brew install mediainfo, Windows/vcpkg: libmediainfo). "
        "Vendored MediaInfo submodules/binaries are not supported.")
else()
    message(STATUS
        "MediaInfoLib not found: using built-in metadata fallback parsers "
        "(audio + MP4/MOV/M4V/3GP tags only).")
endif()

# Vendored document analysis deps
set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external")
set(PUGIXML_DIR "${EXTERNAL_DIR}/pugixml")
set(LIBZIP_DIR "${EXTERNAL_DIR}/libzip")
set(PDFIUM_DIR "${EXTERNAL_DIR}/pdfium")
set(DOC_DEPS_INCLUDE_DIRS "")
set(DOC_DEPS_LIBS "")

# Pugixml (compiled via PugixmlBundle.cpp)
if(EXISTS "${PUGIXML_DIR}/src/pugixml.hpp")
    list(APPEND DOC_DEPS_INCLUDE_DIRS "${PUGIXML_DIR}/src")
    add_compile_definitions(PUGIXML_NO_EXCEPTIONS AI_FILE_SORTER_USE_PUGIXML)
endif()

# libzip (build from vendored source)
if(EXISTS "${LIBZIP_DIR}/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_BZIP2 OFF CACHE BOOL "" FORCE)
    set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
    set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(ENABLE_GNUTLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_COMMONCRYPTO OFF CACHE BOOL "" FORCE)
    set(ENABLE_WINDOWS_CRYPTO OFF CACHE BOOL "" FORCE)
    add_subdirectory("${LIBZIP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libzip")
    if(TARGET libzip::zip)
        add_compile_definitions(AI_FILE_SORTER_USE_LIBZIP)
        list(APPEND DOC_DEPS_INCLUDE_DIRS "${LIBZIP_DIR}/lib" "${CMAKE_CURRENT_BINARY_DIR}/libzip")
        list(APPEND DOC_DEPS_LIBS libzip::zip)
    endif()
endif()

set(PDFIUM_PLATFORM_DIR "")
set(PDFIUM_LIBRARY "")
set(PDFIUM_RUNTIME "")
set(_aifs_pdfium_missing_reason "")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/windows-x64")
        set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/pdfium.dll.lib")
        set(PDFIUM_RUNTIME "${PDFIUM_PLATFORM_DIR}/bin/pdfium.dll")
    else()
        set(_aifs_pdfium_missing_reason
            "Embedded PDFium is only vendored for 64-bit Windows builds.")
    endif()
elseif(APPLE)
    set(_aifs_pdfium_apple_arch "")
    if(CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _aifs_pdfium_arch_count)
        if(_aifs_pdfium_arch_count GREATER 1)
            set(_aifs_pdfium_missing_reason
                "Embedded PDFium is vendored per-architecture. Configure a single macOS architecture instead of a universal build.")
        else()
            list(GET CMAKE_OSX_ARCHITECTURES 0 _aifs_pdfium_apple_arch)
        endif()
    endif()
    if(NOT _aifs_pdfium_apple_arch)
        set(_aifs_pdfium_apple_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    string(TOLOWER "${_aifs_pdfium_apple_arch}" _aifs_pdfium_apple_arch)
    if(_aifs_pdfium_apple_arch MATCHES "^(arm64|aarch64)$")
        set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/macos-arm64")
        set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/libpdfium.dylib")
        set(PDFIUM_RUNTIME "${PDFIUM_LIBRARY}")
    elseif(_aifs_pdfium_apple_arch MATCHES "^(x86_64|amd64)$")
        set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/macos-x64")
        set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/libpdfium.dylib")
        set(PDFIUM_RUNTIME "${PDFIUM_LIBRARY}")
    else()
        set(_aifs_pdfium_missing_reason
            "Embedded PDFium is not vendored for macOS architecture '${_aifs_pdfium_apple_arch}'.")
    endif()
elseif(UNIX)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _aifs_pdfium_linux_arch)
    if(_aifs_pdfium_linux_arch MATCHES "^(x86_64|amd64)$")
        set(PDFIUM_PLATFORM_DIR "${PDFIUM_DIR}/linux-x64")
        set(PDFIUM_LIBRARY "${PDFIUM_PLATFORM_DIR}/lib/libpdfium.so")
        set(PDFIUM_RUNTIME "${PDFIUM_LIBRARY}")
    else()
        set(_aifs_pdfium_missing_reason
            "Embedded PDFium is not vendored for Linux architecture '${CMAKE_SYSTEM_PROCESSOR}'.")
    endif()
endif()

set(_aifs_pdfium_ready FALSE)
if(PDFIUM_PLATFORM_DIR
   AND EXISTS "${PDFIUM_PLATFORM_DIR}/include/fpdfview.h"
   AND EXISTS "${PDFIUM_LIBRARY}"
   AND PDFIUM_RUNTIME
   AND EXISTS "${PDFIUM_RUNTIME}")
    set(_aifs_pdfium_ready TRUE)
endif()

if(_aifs_pdfium_ready)
    add_compile_definitions(AI_FILE_SORTER_USE_PDFIUM)
    list(APPEND DOC_DEPS_INCLUDE_DIRS "${PDFIUM_PLATFORM_DIR}/include")
    add_library(pdfium SHARED IMPORTED)
    if(WIN32)
        set_target_properties(pdfium PROPERTIES
            IMPORTED_IMPLIB "${PDFIUM_LIBRARY}"
            IMPORTED_LOCATION "${PDFIUM_RUNTIME}"
        )
    else()
        set_target_properties(pdfium PROPERTIES
            IMPORTED_LOCATION "${PDFIUM_LIBRARY}"
        )
    endif()
    list(APPEND DOC_DEPS_LIBS pdfium)
elseif(PDFIUM_PLATFORM_DIR OR _aifs_pdfium_missing_reason)
    if(NOT _aifs_pdfium_missing_reason)
        set(_aifs_pdfium_missing_reason
            "Embedded PDFium is incomplete under '${PDFIUM_PLATFORM_DIR}'. Run app/scripts/vendor_doc_deps.sh or app/scripts/vendor_doc_deps.ps1 to populate the vendored runtime files.")
    endif()
    if(AI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND)
        message(FATAL_ERROR "${_aifs_pdfium_missing_reason}")
    else()
        message(WARNING "${_aifs_pdfium_missing_reason} Falling back to 'pdftotext' when available.")
    endif()
endif()

# Sources
set(APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB APP_LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
)
set(AIFS_TEST_ONLY_APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/MainAppTestAccess.cpp"
)
list(REMOVE_ITEM APP_LIB_SOURCES ${AIFS_TEST_ONLY_APP_SOURCES})
file(GLOB APP_HEADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
set(APP_SOURCES ${APP_MAIN_SOURCE} ${APP_LIB_SOURCES})
set(AIFS_TRANSLATION_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_fr.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_de.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_it.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_es.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_nl.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_tr.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/aifilesorter_ko.ts"
)
set(AIFS_TRANSLATION_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/i18n")

# Executable (GUI subsystem on Windows)
if(WIN32)
    add_executable(aifilesorter WIN32 ${APP_SOURCES})
else()
    add_executable(aifilesorter ${APP_SOURCES})
endif()

aifs_apply_update_mode(aifilesorter)

target_include_directories(aifilesorter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd"
    ${DOC_DEPS_INCLUDE_DIRS}
)

qt_add_lupdate(
    aifilesorter
    NO_GLOBAL_TARGET
    TS_FILES ${AIFS_TRANSLATION_TS_FILES}
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/startapp_windows.cpp" ${APP_HEADER_SOURCES}
    OPTIONS -no-obsolete
)

set_source_files_properties(${AIFS_TRANSLATION_TS_FILES}
    PROPERTIES OUTPUT_LOCATION "${AIFS_TRANSLATION_QM_DIR}")

qt_add_lrelease(
    aifilesorter
    TS_FILES ${AIFS_TRANSLATION_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE AIFS_TRANSLATION_QM_FILES
)

set_source_files_properties(${AIFS_TRANSLATION_QM_FILES} PROPERTIES GENERATED TRUE)

function(aifs_add_translation_resources target_name)
    qt_add_resources(${target_name} "${target_name}_translations"
        PREFIX "/i18n"
        BASE "${AIFS_TRANSLATION_QM_DIR}"
        FILES ${AIFS_TRANSLATION_QM_FILES}
    )
endfunction()

aifs_add_translation_resources(aifilesorter)

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

    set(MTMD_IMPORT "${PRECOMPILED_CPU_LIB_DIR}/mtmd.lib")
    set(MTMD_DLL "${PRECOMPILED_CPU_BIN_DIR}/mtmd.dll")
    if(EXISTS "${MTMD_IMPORT}" AND EXISTS "${MTMD_DLL}")
        add_library(mtmd SHARED IMPORTED)
        set_target_properties(mtmd PROPERTIES
            IMPORTED_IMPLIB "${MTMD_IMPORT}"
            IMPORTED_LOCATION "${MTMD_DLL}"
        )
        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_HAS_MTMD)
        set(_aifs_saved_required_libs "${CMAKE_REQUIRED_LIBRARIES}")
        set(CMAKE_REQUIRED_LIBRARIES "${MTMD_IMPORT}")
        check_cxx_source_compiles([[
            extern "C" void mtmd_helper_set_progress_callback(void*, void*);
            int main() {
                mtmd_helper_set_progress_callback(0, 0);
                return 0;
            }
        ]] HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
        check_cxx_source_compiles([[
            extern "C" void mtmd_helper_log_set(void*, void*);
            int main() {
                mtmd_helper_log_set(0, 0);
                return 0;
            }
        ]] HAVE_MTMD_HELPER_LOG_SET)
        set(CMAKE_REQUIRED_LIBRARIES "${_aifs_saved_required_libs}")
        unset(_aifs_saved_required_libs)
        if(HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
            target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
        endif()
        if(HAVE_MTMD_HELPER_LOG_SET)
            target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_LOG_CALLBACK)
        endif()
    else()
        message(WARNING "Visual LLM support disabled: missing mtmd.lib/mtmd.dll under app/lib/precompiled/cpu. Run app/scripts/build_llama_windows.ps1 to stage mtmd.")
    endif()
else()
    # Build llama.cpp from the included submodule for non-Windows platforms
    set(LLAMA_BUILD_COMMON ON CACHE BOOL "llama: build common utils library" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp" "${CMAKE_CURRENT_BINARY_DIR}/llama-build")
    get_directory_property(_llama_install_version DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/llama-build" DEFINITION LLAMA_INSTALL_VERSION)
    if (NOT _llama_install_version OR _llama_install_version STREQUAL "LLAMA_INSTALL_VERSION-NOTFOUND")
        set(_llama_install_version "0.0.0")
    endif()
    set(LLAMA_INSTALL_VERSION "${_llama_install_version}")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd" "${CMAKE_CURRENT_BINARY_DIR}/mtmd-build")
    target_link_libraries(aifilesorter PRIVATE llama)
    target_link_libraries(aifilesorter PRIVATE mtmd)
    target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_HAS_MTMD)
    set(CMAKE_REQUIRED_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/tools/mtmd"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/external/llama.cpp/ggml/include"
    )
    check_symbol_exists(mtmd_helper_set_progress_callback "mtmd-helper.h" HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
    check_symbol_exists(mtmd_helper_log_set "mtmd-helper.h" HAVE_MTMD_HELPER_LOG_SET)
    set(CMAKE_REQUIRED_INCLUDES)
    if(HAVE_MTMD_HELPER_SET_PROGRESS_CALLBACK)
        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
    endif()
    if(HAVE_MTMD_HELPER_LOG_SET)
        target_compile_definitions(aifilesorter PRIVATE AI_FILE_SORTER_MTMD_LOG_CALLBACK)
    endif()
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
    ${MEDIAINFO_DEPS_LIBS}
    ${DOC_DEPS_LIBS}
)

if(WIN32)
    target_link_libraries(aifilesorter PRIVATE ggml ggml_base ggml_cpu)
    if(TARGET mtmd)
        target_link_libraries(aifilesorter PRIVATE mtmd)
    endif()
endif()

if(_aifs_pdfium_ready)
    if(PDFIUM_RUNTIME AND EXISTS "${PDFIUM_RUNTIME}")
        add_custom_command(TARGET aifilesorter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PDFIUM_RUNTIME}"
                "$<TARGET_FILE_DIR:aifilesorter>"
        )
    endif()
    if(NOT WIN32)
        if(APPLE)
            set(_pdfium_rpath "@loader_path")
        else()
            set(_pdfium_rpath "$ORIGIN")
        endif()
        set_target_properties(aifilesorter PROPERTIES
            BUILD_RPATH "${_pdfium_rpath}"
            INSTALL_RPATH "${_pdfium_rpath}"
        )
        unset(_pdfium_rpath)
    endif()
endif()

if(WIN32)
    set(STARTER_TARGET StartAiFileSorter)
    add_executable(${STARTER_TARGET} WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/startapp_windows.cpp"
    )
    aifs_apply_update_mode(${STARTER_TARGET})
    target_include_directories(${STARTER_TARGET} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(${STARTER_TARGET} PRIVATE
        Qt6::Widgets Qt6::Gui Qt6::Core
    )
    target_compile_definitions(${STARTER_TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# On Windows, ensure Unicode and lean Windows headers in deps that need it
if(WIN32)
    target_compile_definitions(aifilesorter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set(WIN_SYSTEM_LIBS wininet d3d11 dxgi dxguid d3d12 mpr userenv cldapi)

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
        if(NOT ${varName})
            message(FATAL_ERROR "Required system library '${libName}' not found. Ensure the Windows SDK is installed.")
        endif()
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

    function(aifs_stage_test_runtime target_name)
        if(WIN32)
            set(_aifs_test_runtime_dlls "")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target_name}>
                    $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
            )

            if(VCPKG_TARGET_TRIPLET)
                set(_aifs_vcpkg_runtime_dir "${AIFS_VCPKG_INSTALLED_ROOT}/${VCPKG_TARGET_TRIPLET}/bin")
                if(EXISTS "${_aifs_vcpkg_runtime_dir}")
                    file(GLOB _aifs_vcpkg_runtime_dlls "${_aifs_vcpkg_runtime_dir}/*.dll")
                    list(APPEND _aifs_test_runtime_dlls ${_aifs_vcpkg_runtime_dlls})
                endif()
            endif()

            if(EXISTS "${PRECOMPILED_CPU_ROOT}/bin")
                file(GLOB _aifs_precompiled_cpu_runtime_dlls
                    "${PRECOMPILED_CPU_ROOT}/bin/*.dll")
                list(APPEND _aifs_test_runtime_dlls ${_aifs_precompiled_cpu_runtime_dlls})
            endif()

            set(_aifs_mingw_runtime_search_paths "")
            if(DEFINED ENV{OPENBLAS_ROOT} AND NOT "$ENV{OPENBLAS_ROOT}" STREQUAL "")
                list(APPEND _aifs_mingw_runtime_search_paths "$ENV{OPENBLAS_ROOT}/bin")
            endif()
            list(APPEND _aifs_mingw_runtime_search_paths "C:/msys64/mingw64/bin")

            foreach(_aifs_mingw_runtime_name IN ITEMS
                libgomp-1.dll
                libgcc_s_seh-1.dll
                libgfortran-5.dll
                libwinpthread-1.dll
                libquadmath-0.dll)
                foreach(_aifs_runtime_path IN LISTS _aifs_mingw_runtime_search_paths)
                    if(EXISTS "${_aifs_runtime_path}/${_aifs_mingw_runtime_name}")
                        list(APPEND _aifs_test_runtime_dlls
                            "${_aifs_runtime_path}/${_aifs_mingw_runtime_name}")
                        break()
                    endif()
                endforeach()
            endforeach()

            list(REMOVE_DUPLICATES _aifs_test_runtime_dlls)
            if(_aifs_test_runtime_dlls)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${_aifs_test_runtime_dlls}
                        $<TARGET_FILE_DIR:${target_name}>
                    COMMAND_EXPAND_LISTS
                )
            endif()

            if(VCPKG_TARGET_TRIPLET)
                set(_aifs_qt_plugin_root "${AIFS_VCPKG_INSTALLED_ROOT}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins")
                foreach(_aifs_qt_plugin_subdir IN ITEMS platforms styles imageformats)
                    set(_aifs_qt_plugin_src_dir "${_aifs_qt_plugin_root}/${_aifs_qt_plugin_subdir}")
                    if(EXISTS "${_aifs_qt_plugin_src_dir}")
                        file(GLOB _aifs_qt_plugin_dlls "${_aifs_qt_plugin_src_dir}/*.dll")
                        if(_aifs_qt_plugin_dlls)
                            add_custom_command(TARGET ${target_name} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E make_directory
                                    "$<TARGET_FILE_DIR:${target_name}>/${_aifs_qt_plugin_subdir}"
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                    ${_aifs_qt_plugin_dlls}
                                    "$<TARGET_FILE_DIR:${target_name}>/${_aifs_qt_plugin_subdir}"
                                COMMAND_EXPAND_LISTS
                            )
                        endif()
                    endif()
                endforeach()
            endif()
        endif()

        if(_aifs_pdfium_ready AND PDFIUM_RUNTIME AND EXISTS "${PDFIUM_RUNTIME}")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${PDFIUM_RUNTIME}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            )
        endif()
        if(_aifs_pdfium_ready AND NOT WIN32)
            if(APPLE)
                set(_pdfium_rpath "@loader_path")
            else()
                set(_pdfium_rpath "$ORIGIN")
            endif()
            set_target_properties(${target_name} PROPERTIES
                BUILD_RPATH "${_pdfium_rpath}"
                INSTALL_RPATH "${_pdfium_rpath}"
            )
            unset(_pdfium_rpath)
        endif()
    endfunction()

    add_executable(aifs_onedrive_storage_plugin
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/onedrive_storage_plugin_main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/CloudPathSupport.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/FileScanner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/LocalFsProvider.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/Logger.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/OneDriveStorageProvider.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/Utils.cpp"
    )
    target_include_directories(aifs_onedrive_storage_plugin PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
    )
    target_link_libraries(aifs_onedrive_storage_plugin PRIVATE
        Qt6::Core
        CURL::libcurl
        spdlog::spdlog
        fmt::fmt
    )
    if(WIN32)
        target_compile_definitions(aifs_onedrive_storage_plugin PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(aifs_onedrive_storage_plugin PRIVATE ${WIN_SYSTEM_LIBS})
    endif()
    set_target_properties(aifs_onedrive_storage_plugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_compile_definitions(aifilesorter PRIVATE
        AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME="$<TARGET_FILE_NAME:aifs_onedrive_storage_plugin>"
    )
    add_dependencies(aifilesorter aifs_onedrive_storage_plugin)

    add_executable(ai_file_sorter_tests
        ${APP_LIB_SOURCES}
        ${AIFS_TEST_ONLY_APP_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_utils.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_file_scanner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_local_llm_backend.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_ggml_runtime_paths.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llm_downloader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_update_feed.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_updater.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_updater_build_modes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llm_selection_dialog_visual.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_translation.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_image_options.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_storage_support.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_main_app_visual_fallback.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_settings_image_options.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_ui_translator.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_storage_plugin_dialog.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_categorization_dialog.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_checkbox_matrix.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_review_dialog_rename_gate.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_cli_reporter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_support_prompt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_whitelist_and_prompt.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_database_manager_rename_only.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_cache_interactions.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_image_rename_metadata_service.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_llava_image_analyzer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_media_rename_metadata_service.cpp"
    )

    add_executable(aifs_storage_plugin_stub
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/helpers/storage_plugin_stub.cpp"
    )
    target_link_libraries(aifs_storage_plugin_stub PRIVATE Qt6::Core)
    set_target_properties(aifs_storage_plugin_stub PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_include_directories(ai_file_sorter_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/llama"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit"
        ${DOC_DEPS_INCLUDE_DIRS}
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
        ${MEDIAINFO_DEPS_LIBS}
        ${DOC_DEPS_LIBS}
    )

    aifs_add_translation_resources(ai_file_sorter_tests)

    target_compile_definitions(ai_file_sorter_tests PRIVATE
        AI_FILE_SORTER_TEST_BUILD=1
        AIFS_ONEDRIVE_STORAGE_PLUGIN_NAME="$<TARGET_FILE_NAME:aifs_onedrive_storage_plugin>"
        AIFS_STORAGE_PLUGIN_STUB_NAME="$<TARGET_FILE_NAME:aifs_storage_plugin_stub>"
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_dependencies(ai_file_sorter_tests aifs_onedrive_storage_plugin)
    add_dependencies(ai_file_sorter_tests aifs_storage_plugin_stub)
    aifs_apply_update_mode(ai_file_sorter_tests)
    aifs_apply_expected_update_mode(ai_file_sorter_tests "${_aifs_effective_update_mode}")

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

    aifs_stage_test_runtime(ai_file_sorter_tests)

    if(WIN32)
        set(AIFS_UPDATER_MODE_TEST_APP_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/IniConfig.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/Logger.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/Settings.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/UpdateArchiveExtractor.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/UpdateFeed.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/UpdateInstaller.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/Updater.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/Utils.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/Version.cpp"
        )

        function(aifs_add_updater_mode_test target_name update_mode)
            add_executable(${target_name}
                ${AIFS_UPDATER_MODE_TEST_APP_SOURCES}
                "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_updater_build_modes.cpp"
            )

            target_include_directories(${target_name} PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit"
                ${DOC_DEPS_INCLUDE_DIRS}
            )

            target_link_libraries(${target_name} PRIVATE
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
                ${DOC_DEPS_LIBS}
            )

            target_compile_definitions(${target_name} PRIVATE AI_FILE_SORTER_TEST_BUILD=1 WIN32_LEAN_AND_MEAN NOMINMAX)
            foreach(libName IN LISTS WIN_SYSTEM_LIBS)
                string(TOUPPER "${libName}" upperLib)
                set(varName "${upperLib}_LIBRARY")
                if(DEFINED ${varName} AND ${varName})
                    target_link_libraries(${target_name} PRIVATE "${${varName}}")
                else()
                    target_link_libraries(${target_name} PRIVATE ${libName})
                endif()
            endforeach()

            aifs_apply_named_update_mode(${target_name} "${update_mode}")
            aifs_apply_expected_update_mode(${target_name} "${update_mode}")
            aifs_stage_test_runtime(${target_name})
        endfunction()

        aifs_add_updater_mode_test(ai_file_sorter_updater_notify_only_tests "NOTIFY_ONLY")
        aifs_add_updater_mode_test(ai_file_sorter_updater_disabled_tests "DISABLED")
    endif()

    if(BUILD_TESTING)
        add_test(NAME ai_file_sorter_tests COMMAND ai_file_sorter_tests)
        if(WIN32)
            add_test(NAME ai_file_sorter_updater_notify_only_tests COMMAND ai_file_sorter_updater_notify_only_tests)
            add_test(NAME ai_file_sorter_updater_disabled_tests COMMAND ai_file_sorter_updater_disabled_tests)
        endif()
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

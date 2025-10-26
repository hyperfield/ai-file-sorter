cmake_minimum_required(VERSION 3.21)

project(AIFileSorter LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer MSVC on Windows if available
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Third‑party deps (resolved best via vcpkg on Windows)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(JsonCpp REQUIRED CONFIG)
find_package(fmt REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(Intl REQUIRED) # libintl/gettext

# Sources
file(GLOB APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
)

# Executable (GUI on Windows)
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
qt_add_resources(aifilesorter "app_resources"
    PREFIX "/net/quicknode/AIFileSorter"
    FILES
        resources/.env
        resources/images/logo.png
        resources/images/qn_logo.png
        resources/images/app_icon_128.png
        resources/images/icon_512x512.png
        resources/certs/cacert.pem
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

# On Windows, ensure Unicode and lean Windows headers in deps that need it
if(WIN32)
    target_compile_definitions(aifilesorter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set(WIN_SYSTEM_LIBS wininet d3d11 dxgi dxguid d3d12 mpr userenv)
    foreach(libName IN LISTS WIN_SYSTEM_LIBS)
        string(TOUPPER "${libName}" upperLib)
        set(varName "${upperLib}_LIBRARY")
        find_library(${varName} NAMES ${libName})
        if(NOT ${varName})
            message(FATAL_ERROR "Required system library '${libName}' not found. Ensure the Windows SDK is installed.")
        endif()
        target_link_libraries(aifilesorter PRIVATE "${${varName}}")
    endforeach()
endif()

# Output dir under app/bin similar to Makefile
set_target_properties(aifilesorter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

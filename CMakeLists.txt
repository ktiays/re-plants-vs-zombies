cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(PlantsVsZombies LANGUAGES C CXX)

option(
    PVZ_BUILD_PORTABLE_ENGINE
    "Build the platform-neutral engine API, core, and tests."
    ON
)
option(
    PVZ_BUILD_LEGACY_WINDOWS
    "Build the reconstructed Win32/Direct3D game used as the parity reference."
    ${WIN32}
)
option(
    PVZ_BUILD_MACOS_APP
    "Build the SDL platform and Metal renderer application for macOS."
    OFF
)

if(PVZ_BUILD_PORTABLE_ENGINE)
    include(CTest)
    add_subdirectory(engine)
    add_subdirectory(game)
endif()

if(PVZ_BUILD_MACOS_APP)
    if(NOT APPLE)
        message(FATAL_ERROR "PVZ_BUILD_MACOS_APP requires macOS.")
    endif()
    if(NOT PVZ_BUILD_PORTABLE_ENGINE)
        message(
            FATAL_ERROR
            "PVZ_BUILD_MACOS_APP requires PVZ_BUILD_PORTABLE_ENGINE."
        )
    endif()
    enable_language(OBJCXX)
    add_subdirectory(platform/macos)
endif()

if(PVZ_BUILD_LEGACY_WINDOWS)
    if(NOT WIN32)
        message(
            FATAL_ERROR
            "PVZ_BUILD_LEGACY_WINDOWS requires a Windows target toolchain."
        )
    endif()

    set(
        CMAKE_CONFIGURATION_TYPES
        "Debug"
        "Release"
        CACHE STRING "List of supported configurations." FORCE
    )
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Preserve the reconstructed Windows target's current compiler settings while
    # the legacy implementation is moved behind portable engine interfaces.
    if(MSVC)
        set(universal_compiler_flags "")
        set(warn_flags "")
    else()
        set(
            universal_compiler_flags
            "-pthread -fuse-ld=lld -fms-extensions -march=native -fdiagnostics-color=always -O3"
        )
        set(warn_flags "-Wall -Wextra -Werror")
    endif()

    set(universal_cxxflags "${CMAKE_CXX_FLAGS} ${universal_compiler_flags}")
    set(universal_cflags "${CMAKE_C_FLAGS} ${universal_compiler_flags}")
    set(CMAKE_CXX_FLAGS "${universal_cxxflags} ${warn_flags}")
    set(CMAKE_C_FLAGS "${universal_cflags} ${warn_flags}")
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    enable_language(RC)

    function(use_package TARGET PACKAGE VERSION)
        message(
            WARNING
            "No implementation of use_package. Package \"${PACKAGE}\" "
            "version \"${VERSION}\" for target \"${TARGET}\" is ignored."
        )
    endfunction()

    include(CMake/Utils.cmake)
    include(CMake/GlobalSettingsInclude.cmake OPTIONAL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    add_subdirectory(SexyAppFramework)
endif()

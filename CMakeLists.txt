cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(PlantsVsZombies LANGUAGES C CXX)

if(MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:/utf-8>"
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

option(
    PVZ_BUILD_GAME
    "Build the cross-platform game: the reconstructed SexyAppFramework on SDL3 with a native rendering backend."
    ON
)
option(
    PVZ_BUILD_PORTABLE_ENGINE
    "Build the platform-neutral engine API, core, and tests."
    ON
)
option(
    PVZ_BUILD_MACOS_APP
    "Build the SDL platform and Metal renderer application for macOS."
    OFF
)
option(
    PVZ_BUILD_IMAGE_CODECS
    "Build the portable PNG and JPEG image decoder."
    ${PVZ_BUILD_MACOS_APP}
)
option(
    PVZ_BUILD_AUDIO_CODECS
    "Build the portable Ogg Vorbis sound decoder."
    ${PVZ_BUILD_MACOS_APP}
)
option(
    PVZ_BUILD_MODULE_MUSIC
    "Build MO3 module music support through libopenmpt."
    ${PVZ_BUILD_MACOS_APP}
)

if(PVZ_BUILD_MODULE_MUSIC)
    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")
    find_package(OpenMPT REQUIRED)
endif()

if(PVZ_BUILD_PORTABLE_ENGINE)
    include(CTest)
    add_subdirectory(engine)
    add_subdirectory(game)
    add_subdirectory(parity)
    add_subdirectory(platform/windows/reference)
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

if(PVZ_BUILD_GAME)
    add_subdirectory(SexyAppFramework)
endif()

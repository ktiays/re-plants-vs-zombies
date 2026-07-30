find_package(SDL2 CONFIG REQUIRED)

find_library(PVZ_METAL_FRAMEWORK Metal REQUIRED)
find_library(PVZ_QUARTZ_CORE_FRAMEWORK QuartzCore REQUIRED)
find_library(PVZ_FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_program(PVZ_XCRUN_EXECUTABLE xcrun REQUIRED)
if(NOT TARGET pvz_image_codecs)
    message(
        FATAL_ERROR
        "PVZ_BUILD_MACOS_APP requires PVZ_BUILD_IMAGE_CODECS."
    )
endif()
if(NOT TARGET pvz_audio_codecs)
    message(
        FATAL_ERROR
        "PVZ_BUILD_MACOS_APP requires PVZ_BUILD_AUDIO_CODECS."
    )
endif()
if(NOT TARGET OpenMPT::OpenMPT)
    message(
        FATAL_ERROR
        "PVZ_BUILD_MACOS_APP requires PVZ_BUILD_MODULE_MUSIC."
    )
endif()

set(PVZ_METAL_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/PvzShaders.metallib")
set(
    PVZ_METAL_MODULE_CACHE
    "${CMAKE_CURRENT_BINARY_DIR}/metal-module-cache"
)
add_custom_command(
    OUTPUT "${PVZ_METAL_LIBRARY}"
    COMMAND
        "${CMAKE_COMMAND}"
        -E env
        "CLANG_MODULE_CACHE_PATH=${PVZ_METAL_MODULE_CACHE}"
        "${PVZ_XCRUN_EXECUTABLE}"
        -sdk macosx
        metal
        "-fmodules-cache-path=${PVZ_METAL_MODULE_CACHE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/PvzShaders.metal"
        -o "${PVZ_METAL_LIBRARY}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/PvzShaders.metal"
    VERBATIM
)
add_custom_target(pvz_metal_shaders DEPENDS "${PVZ_METAL_LIBRARY}")

add_library(
    pvz_platform_sdl
    STATIC
        src/SdlAudioDevice.cpp
        src/SdlPlatform.cpp
)
target_compile_features(pvz_platform_sdl PUBLIC cxx_std_23)
target_include_directories(
    pvz_platform_sdl
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    pvz_platform_sdl
    PUBLIC
        pvz_engine_api
    PRIVATE
        OpenMPT::OpenMPT
        SDL2::SDL2
)

add_library(
    pvz_renderer_metal
    STATIC
        src/MetalRenderDevice.mm
)
target_compile_features(pvz_renderer_metal PUBLIC cxx_std_23)
target_include_directories(
    pvz_renderer_metal
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(pvz_renderer_metal PRIVATE -fobjc-arc)
target_link_libraries(
    pvz_renderer_metal
    PUBLIC
        pvz_engine_api
    PRIVATE
        "${PVZ_METAL_FRAMEWORK}"
        "${PVZ_QUARTZ_CORE_FRAMEWORK}"
        "${PVZ_FOUNDATION_FRAMEWORK}"
)

add_executable(
    pvz_app_macos
    MACOSX_BUNDLE
        src/main.mm
        src/RendererSmokeGame.cpp
        "${PVZ_METAL_LIBRARY}"
)
set_source_files_properties(
    "${PVZ_METAL_LIBRARY}"
    PROPERTIES
        GENERATED TRUE
        MACOSX_PACKAGE_LOCATION Resources
)
add_dependencies(pvz_app_macos pvz_metal_shaders)
target_compile_features(pvz_app_macos PRIVATE cxx_std_23)
target_compile_definitions(pvz_app_macos PRIVATE SDL_MAIN_HANDLED)
target_compile_options(pvz_app_macos PRIVATE -fobjc-arc)
target_link_libraries(
    pvz_app_macos
    PRIVATE
        pvz_game
        pvz_audio_codecs
        pvz_engine_core
        pvz_image_codecs
        pvz_platform_sdl
        pvz_renderer_metal
        SDL2::SDL2
        "${PVZ_FOUNDATION_FRAMEWORK}"
)
set_target_properties(
    pvz_app_macos
    PROPERTIES
        OUTPUT_NAME "PlantsVsZombies"
        MACOSX_BUNDLE_BUNDLE_NAME "Plants vs. Zombies"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.ktiays.plants-vs-zombies"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

foreach(
    PVZ_MACOS_TARGET
    IN ITEMS
        pvz_platform_sdl
        pvz_renderer_metal
        pvz_app_macos
)
    if(MSVC)
        target_compile_options(
            "${PVZ_MACOS_TARGET}"
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            "${PVZ_MACOS_TARGET}"
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
    endif()
endforeach()

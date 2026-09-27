################################################################################
# Plants vs. Zombies: the reconstructed game on the portable SexyAppFramework
#
# Platform services (window, input, timing, audio output) come from SDL3. The
# renderer is selected per platform behind Sexy::RenderDevice; see
# graphics/RenderDevice.h.
################################################################################

set(PVZ_GAME_TARGET PlantsVsZombies)

find_package(SDL3 CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")
find_package(OpenMPT REQUIRED)

set(PVZ_GAME_SOURCES
    # Game: board
    ../Lawn/Board.cpp
    ../Lawn/Challenge.cpp
    ../Lawn/Coin.cpp
    ../Lawn/CursorObject.cpp
    ../Lawn/CutScene.cpp
    ../Lawn/GameObject.cpp
    ../Lawn/GridItem.cpp
    ../Lawn/LawnMower.cpp
    ../Lawn/MessageWidget.cpp
    ../Lawn/Plant.cpp
    ../Lawn/Projectile.cpp
    ../Lawn/SeedPacket.cpp
    ../Lawn/ToolTipWidget.cpp
    ../Lawn/ZenGarden.cpp
    ../Lawn/Zombie.cpp

    # Game: common
    ../Lawn/LawnCommon.cpp
    ../Resources.cpp
    ../LawnApp.cpp
    ../main.cpp

    # Game: system
    ../Lawn/System/DataSync.cpp
    ../Lawn/System/LegacySaveFormat.cpp
    ../Lawn/System/Music.cpp
    ../Lawn/System/PlayerInfo.cpp
    ../Lawn/System/PlayerInfoSerialization.cpp
    ../Lawn/System/PoolEffect.cpp
    ../Lawn/System/PopDRMComm.cpp
    ../Lawn/System/ProfileMgr.cpp
    ../Lawn/System/ReanimationLawn.cpp
    ../Lawn/System/SaveGame.cpp
    ../Lawn/System/TypingCheck.cpp

    # Game: widgets
    ../Lawn/Widget/AchievementsWidget.cpp
    ../Lawn/Widget/Almanac.cpp
    ../Lawn/Widget/AwardScreen.cpp
    ../Lawn/Widget/ChallengeScreen.cpp
    ../Lawn/Widget/CheatDialog.cpp
    ../Lawn/Widget/ContinueDialog.cpp
    ../Lawn/Widget/CreditScreen.cpp
    ../Lawn/Widget/GameButton.cpp
    ../Lawn/Widget/GameSelector.cpp
    ../Lawn/Widget/ImitaterDialog.cpp
    ../Lawn/Widget/LawnDialog.cpp
    ../Lawn/Widget/NewOptionsDialog.cpp
    ../Lawn/Widget/NewUserDialog.cpp
    ../Lawn/Widget/SeedChooserScreen.cpp
    ../Lawn/Widget/StoreScreen.cpp
    ../Lawn/Widget/TitleScreen.cpp
    ../Lawn/Widget/UserDialog.cpp
    ../Lawn/Widget/ZombatarColors.cpp
    ../Lawn/Widget/ZombatarItems.cpp
    ../Lawn/Widget/ZombatarScreen.cpp
    ../Lawn/Widget/ZombatarTOSDialog.cpp

    # TodLib
    ../Sexy.TodLib/Attachment.cpp
    ../Sexy.TodLib/Definition.cpp
    ../Sexy.TodLib/EffectSystem.cpp
    ../Sexy.TodLib/FilterEffect.cpp
    ../Sexy.TodLib/ReanimAtlas.cpp
    ../Sexy.TodLib/Reanimator.cpp
    ../Sexy.TodLib/TodCommon.cpp
    ../Sexy.TodLib/TodDebug.cpp
    ../Sexy.TodLib/TodFoley.cpp
    ../Sexy.TodLib/TodList.cpp
    ../Sexy.TodLib/TodParticle.cpp
    ../Sexy.TodLib/TodStringFile.cpp
    ../Sexy.TodLib/Trail.cpp
)

set(PVZ_FRAMEWORK_SOURCES
    Common.cpp
    SexyApp.cpp
    SexyAppBase.cpp

    graphics/Color.cpp
    graphics/DeviceImage.cpp
    graphics/DisplayInterface.cpp
    graphics/Font.cpp
    graphics/Graphics.cpp
    graphics/HardwareRenderer.cpp
    graphics/Image.cpp
    graphics/ImageFont.cpp
    graphics/MemoryImage.cpp
    graphics/NativeDisplay.cpp
    graphics/Quantize.cpp
    graphics/SharedImage.cpp
    graphics/SWTri.cpp
    graphics/SysFont.cpp

    imagelib/ImageLib.cpp

    misc/Buffer.cpp
    misc/CritSect.cpp
    misc/Debug.cpp
    misc/DescParser.cpp
    misc/Flags.cpp
    misc/KeyCodes.cpp
    misc/ModVal.cpp
    misc/MTRand.cpp
    misc/PerfTimer.cpp
    misc/PropertiesParser.cpp
    misc/Ratio.cpp
    misc/RegistryStore.cpp
    misc/ResourceManager.cpp
    misc/SexyMatrix.cpp
    misc/XMLParser.cpp

    paklib/PakInterface.cpp

    sound/AudioMixer.cpp
    sound/MixerSoundInstance.cpp
    sound/MixerSoundManager.cpp
    sound/ModuleMusicInterface.cpp
    sound/OggDecoder.cpp

    widget/ButtonWidget.cpp
    widget/Checkbox.cpp
    widget/Dialog.cpp
    widget/DialogButton.cpp
    widget/EditWidget.cpp
    widget/HyperlinkWidget.cpp
    widget/Insets.cpp
    widget/ListWidget.cpp
    widget/ScrollbarWidget.cpp
    widget/ScrollbuttonWidget.cpp
    widget/Slider.cpp
    widget/TextWidget.cpp
    widget/Widget.cpp
    widget/WidgetContainer.cpp
    widget/WidgetManager.cpp

    platform/sdl/SDLPlatform.cpp
    platform/sdl/SDLAudioDevice.cpp
)

# Tremor (integer Ogg Vorbis decoder) used for sound effects and streams.
add_library(pvz_tremor STATIC
    ogg/bitwise.c
    ogg/block.c
    ogg/codebook.c
    ogg/floor0.c
    ogg/floor1.c
    ogg/framing.c
    ogg/info.c
    ogg/mapping0.c
    ogg/mdct.c
    ogg/registry.c
    ogg/res012.c
    ogg/sharedbook.c
    ogg/synthesis.c
    ogg/vorbisfile.c
    ogg/window.c
)
target_include_directories(pvz_tremor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ogg")
if(NOT MSVC)
    target_compile_options(pvz_tremor PRIVATE -w)
endif()

# Rendering backends. Each platform provides one Sexy::RenderDevice.
if(APPLE)
    enable_language(OBJCXX)
    set(PVZ_RENDER_BACKEND_SOURCES graphics/metal/MetalRenderDevice.mm)
    find_library(PVZ_METAL_FRAMEWORK Metal REQUIRED)
    find_library(PVZ_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(PVZ_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(PVZ_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    set(PVZ_RENDER_BACKEND_LIBRARIES
        ${PVZ_METAL_FRAMEWORK}
        ${PVZ_QUARTZCORE_FRAMEWORK}
        ${PVZ_FOUNDATION_FRAMEWORK}
        ${PVZ_COREGRAPHICS_FRAMEWORK}
    )
    set(PVZ_RENDER_BACKEND_DEFINITIONS SEXY_RENDER_BACKEND_METAL=1)
    set_source_files_properties(graphics/metal/MetalRenderDevice.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
    message(FATAL_ERROR "No rendering backend is available for this platform yet.")
endif()

add_executable(${PVZ_GAME_TARGET}
    ${PVZ_GAME_SOURCES}
    ${PVZ_FRAMEWORK_SOURCES}
    ${PVZ_RENDER_BACKEND_SOURCES}
)

target_compile_features(${PVZ_GAME_TARGET} PRIVATE cxx_std_23)
target_include_directories(${PVZ_GAME_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(${PVZ_GAME_TARGET} PRIVATE
    ${PVZ_RENDER_BACKEND_DEFINITIONS}
    SEXY_DISABLE_MODVAL
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)
if(NOT MSVC)
    target_compile_options(${PVZ_GAME_TARGET} PRIVATE
        -Wno-unknown-pragmas
        -Wno-deprecated-declarations
        -Wno-invalid-offsetof
        -Wno-format-security
        -Wno-dangling-else
        -Wno-parentheses
        -Wno-switch
        -fno-strict-aliasing
    )
endif()

target_link_libraries(${PVZ_GAME_TARGET} PRIVATE
    SDL3::SDL3
    PNG::PNG
    JPEG::JPEG
    ZLIB::ZLIB
    OpenMPT::OpenMPT
    pvz_tremor
    ${PVZ_RENDER_BACKEND_LIBRARIES}
)

if(APPLE)
    set_target_properties(${PVZ_GAME_TARGET} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Plants vs. Zombies"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.ktiays.plants-vs-zombies"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/Info.plist.in"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.2.0.1073"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.2.0.1073"
    )
endif()

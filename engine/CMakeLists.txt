add_library(pvz_engine_api INTERFACE)
target_compile_features(pvz_engine_api INTERFACE cxx_std_23)
target_include_directories(
    pvz_engine_api
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/api/include"
)

add_library(
    pvz_engine_core
    STATIC
        core/src/ApplicationRunner.cpp
        core/src/BinaryStateIO.cpp
        core/src/BitmapFontDescriptor.cpp
        core/src/BitmapFontResourceManager.cpp
        core/src/DeterministicHash.cpp
        core/src/ImageResourceManager.cpp
        core/src/InputReplay.cpp
        core/src/ModuleMusicResourceManager.cpp
        core/src/NullFontResources.cpp
        core/src/NullImageStore.cpp
        core/src/NullMusicResources.cpp
        core/src/NullSoundResources.cpp
        core/src/PakArchive.cpp
        core/src/PakResourceStore.cpp
        core/src/ReplaySession.cpp
        core/src/ResourceXmlDocumentLoader.cpp
        core/src/SoundResourceManager.cpp
        core/src/XmlDocument.cpp
        core/src/XmlPullParser.cpp
)
target_compile_features(pvz_engine_core PUBLIC cxx_std_23)
target_include_directories(
    pvz_engine_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
)
target_link_libraries(pvz_engine_core PUBLIC pvz_engine_api)

if(PVZ_BUILD_IMAGE_CODECS)
    find_package(PNG REQUIRED)
    find_package(JPEG REQUIRED)
    find_package(GIF REQUIRED)

    add_library(
        pvz_image_codecs
        STATIC
            image/src/PortableImageDecoder.cpp
    )
    target_compile_features(pvz_image_codecs PUBLIC cxx_std_23)
    target_include_directories(
        pvz_image_codecs
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/image/include"
    )
    target_link_libraries(
        pvz_image_codecs
        PUBLIC
            pvz_engine_api
        PRIVATE
            PNG::PNG
            JPEG::JPEG
            GIF::GIF
    )
endif()

if(PVZ_BUILD_AUDIO_CODECS)
    # The legacy Tremor ABI is confined to pvz_tremor. Only the fixed-width
    # adapter and engine audio protocol are visible to portable consumers.
    add_library(
        pvz_tremor
        STATIC
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/bitwise.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/block.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/codebook.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/floor0.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/floor1.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/framing.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/info.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/mapping0.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/mdct.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/registry.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/res012.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/sharedbook.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/synthesis.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/vorbisfile.c"
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg/window.c"
            "${PROJECT_SOURCE_DIR}/third_party/tremor_adapter/pvz_tremor_adapter.c"
    )
    target_include_directories(
        pvz_tremor
        PUBLIC
            "${PROJECT_SOURCE_DIR}/third_party/tremor_adapter"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/SexyAppFramework/ogg"
    )

    add_library(
        pvz_audio_codecs
        STATIC
            audio/src/PortableAudioDecoder.cpp
    )
    target_compile_features(pvz_audio_codecs PUBLIC cxx_std_23)
    target_include_directories(
        pvz_audio_codecs
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/audio/include"
    )
    target_link_libraries(
        pvz_audio_codecs
        PUBLIC
            pvz_engine_api
        PRIVATE
            pvz_tremor
    )
endif()

if(MSVC)
    target_compile_options(pvz_engine_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(
        pvz_engine_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()

if(TARGET pvz_image_codecs)
    if(MSVC)
        target_compile_options(
            pvz_image_codecs
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_image_codecs
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
    endif()
endif()

if(TARGET pvz_audio_codecs)
    if(MSVC)
        target_compile_options(
            pvz_audio_codecs
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_audio_codecs
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
    endif()
endif()
add_custom_target(
    pvz_check_portable_boundaries ALL
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPVZ_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/CMake/CheckPortableBoundaries.cmake"
    COMMENT "Checking portable source boundaries"
    VERBATIM
)
add_dependencies(pvz_engine_core pvz_check_portable_boundaries)

add_executable(
    pvz_pak_inspect
    tools/PakInspect.cpp
)
target_compile_features(pvz_pak_inspect PRIVATE cxx_std_23)
target_link_libraries(pvz_pak_inspect PRIVATE pvz_engine_core)

add_executable(
    pvz_replay_inspect
    tools/ReplayInspect.cpp
)
target_compile_features(
    pvz_replay_inspect
    PRIVATE cxx_std_23
)
target_link_libraries(
    pvz_replay_inspect
    PRIVATE pvz_engine_core
)
if(MSVC)
    target_compile_options(
        pvz_replay_inspect
        PRIVATE /W4 /WX /permissive-
    )
else()
    target_compile_options(
        pvz_replay_inspect
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()
if(TARGET pvz_image_codecs)
    target_compile_definitions(
        pvz_pak_inspect
        PRIVATE PVZ_HAS_IMAGE_CODECS=1
    )
    target_link_libraries(
        pvz_pak_inspect
        PRIVATE pvz_image_codecs
    )
endif()
if(TARGET pvz_audio_codecs)
    target_compile_definitions(
        pvz_pak_inspect
        PRIVATE PVZ_HAS_AUDIO_CODECS=1
    )
    target_link_libraries(
        pvz_pak_inspect
        PRIVATE pvz_audio_codecs
    )
endif()
if(TARGET OpenMPT::OpenMPT)
    target_compile_definitions(
        pvz_pak_inspect
        PRIVATE PVZ_HAS_MODULE_MUSIC=1
    )
    target_link_libraries(
        pvz_pak_inspect
        PRIVATE OpenMPT::OpenMPT
    )
endif()

if(BUILD_TESTING)
    add_executable(
        pvz_engine_core_tests
        tests/BinaryStateIOTests.cpp
        tests/ApplicationRunnerTests.cpp
        tests/BitmapFontResourceManagerTests.cpp
        tests/EngineApiContractTests.cpp
        tests/ImageResourceManagerTests.cpp
        tests/InputReplayTests.cpp
        tests/ModuleMusicResourceManagerTests.cpp
        tests/PakArchiveTests.cpp
        tests/SoundResourceManagerTests.cpp
        tests/XmlDocumentTests.cpp
        tests/XmlPullParserTests.cpp
    )
    target_compile_features(pvz_engine_core_tests PRIVATE cxx_std_23)
    target_link_libraries(pvz_engine_core_tests PRIVATE pvz_engine_core)

    if(MSVC)
        target_compile_options(
            pvz_engine_core_tests
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_engine_core_tests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
    endif()

    add_test(NAME pvz_engine_core_tests COMMAND pvz_engine_core_tests)
    add_test(
        NAME pvz_portable_boundaries
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPVZ_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/CMake/CheckPortableBoundaries.cmake"
    )

    if(TARGET pvz_image_codecs)
        add_executable(
            pvz_image_codec_tests
            tests/PortableImageDecoderTests.cpp
        )
        target_compile_features(
            pvz_image_codec_tests
            PRIVATE cxx_std_23
        )
        target_link_libraries(
            pvz_image_codec_tests
            PRIVATE
                pvz_image_codecs
                PNG::PNG
                JPEG::JPEG
                GIF::GIF
        )
        if(MSVC)
            target_compile_options(
                pvz_image_codec_tests
                PRIVATE /W4 /WX /permissive-
            )
        else()
            target_compile_options(
                pvz_image_codec_tests
                PRIVATE
                    -Wall
                    -Wextra
                    -Wpedantic
                    -Wconversion
                    -Wsign-conversion
                    -Werror
            )
        endif()
        add_test(
            NAME pvz_image_codec_tests
            COMMAND pvz_image_codec_tests
        )
    endif()

    if(TARGET pvz_audio_codecs)
        add_executable(
            pvz_audio_codec_tests
            tests/PortableAudioDecoderTests.cpp
        )
        target_compile_features(
            pvz_audio_codec_tests
            PRIVATE cxx_std_23
        )
        target_link_libraries(
            pvz_audio_codec_tests
            PRIVATE pvz_audio_codecs
        )
        if(MSVC)
            target_compile_options(
                pvz_audio_codec_tests
                PRIVATE /W4 /WX /permissive-
            )
        else()
            target_compile_options(
                pvz_audio_codec_tests
                PRIVATE
                    -Wall
                    -Wextra
                    -Wpedantic
                    -Wconversion
                    -Wsign-conversion
                    -Werror
            )
        endif()
        add_test(
            NAME pvz_audio_codec_tests
            COMMAND pvz_audio_codec_tests
        )
    endif()
endif()

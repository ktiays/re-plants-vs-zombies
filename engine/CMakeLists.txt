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
        core/src/ImageResourceManager.cpp
        core/src/NullImageStore.cpp
        core/src/PakArchive.cpp
        core/src/PakResourceStore.cpp
        core/src/ResourceXmlDocumentLoader.cpp
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

if(BUILD_TESTING)
    add_executable(
        pvz_engine_core_tests
        tests/BinaryStateIOTests.cpp
        tests/ApplicationRunnerTests.cpp
        tests/EngineApiContractTests.cpp
        tests/ImageResourceManagerTests.cpp
        tests/PakArchiveTests.cpp
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
endif()

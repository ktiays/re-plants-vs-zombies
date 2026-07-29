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
        core/src/BinaryStateIO.cpp
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

if(BUILD_TESTING)
    add_executable(
        pvz_engine_core_tests
        tests/BinaryStateIOTests.cpp
        tests/EngineApiContractTests.cpp
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
endif()

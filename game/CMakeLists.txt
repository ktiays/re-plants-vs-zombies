add_library(
    pvz_legacy_profile_io
    STATIC
        ../Lawn/System/DataSync.cpp
        ../Lawn/System/PlayerInfoSerialization.cpp
)
target_compile_features(pvz_legacy_profile_io PUBLIC cxx_std_23)
target_include_directories(
    pvz_legacy_profile_io
    PUBLIC
        "${PROJECT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(
        pvz_legacy_profile_io
        PRIVATE /W4 /WX /permissive-
    )
else()
    target_compile_options(
        pvz_legacy_profile_io
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()

add_dependencies(pvz_legacy_profile_io pvz_check_portable_boundaries)

add_library(
    pvz_game
    STATIC
        ../Lawn/System/LegacySaveFormat.cpp
        src/BoardGeometry.cpp
        src/DefinitionLoader.cpp
        src/GameFlow.cpp
        src/GameModule.cpp
        src/LevelOneBoard.cpp
        src/LevelOneCombat.cpp
        src/LevelOneRandomDecision.cpp
        src/ParameterTrack.cpp
        src/ParticleDefinition.cpp
        src/ReanimationDefinition.cpp
        src/ReanimationPlayer.cpp
        src/TrailDefinition.cpp
)
target_compile_features(pvz_game PUBLIC cxx_std_23)
target_include_directories(
    pvz_game
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(pvz_game PUBLIC pvz_engine_api)

if(MSVC)
    target_compile_options(pvz_game PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(
        pvz_game
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()

add_dependencies(pvz_game pvz_check_portable_boundaries)

add_executable(
    pvz_game_headless
    headless/main.cpp
)
target_compile_features(pvz_game_headless PRIVATE cxx_std_23)
target_link_libraries(
    pvz_game_headless
    PRIVATE
        pvz_game
        pvz_parity
)

add_executable(
    pvz_definition_inspect
    tools/DefinitionInspect.cpp
)
target_compile_features(pvz_definition_inspect PRIVATE cxx_std_23)
target_link_libraries(
    pvz_definition_inspect
    PRIVATE
        pvz_game
        pvz_engine_core
)
add_dependencies(
    pvz_definition_inspect
    pvz_check_portable_boundaries
)
if(MSVC)
    target_compile_options(
        pvz_definition_inspect
        PRIVATE /W4 /WX /permissive-
    )
else()
    target_compile_options(
        pvz_definition_inspect
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()

if(BUILD_TESTING)
    add_executable(
        pvz_game_tests
        tests/GameModuleTests.cpp
        tests/DefinitionLoaderTests.cpp
        tests/GameFlowTests.cpp
        tests/LegacyDataSyncTests.cpp
        tests/LegacySaveFormatTests.cpp
        tests/LevelOneBoardTests.cpp
        tests/LevelOneCombatTests.cpp
        tests/ParameterTrackTests.cpp
        tests/ParticleDefinitionTests.cpp
        tests/PlayerInfoSerializationTests.cpp
        tests/ReanimationDefinitionTests.cpp
        tests/ReanimationPlayerTests.cpp
        tests/TrailDefinitionTests.cpp
    )
    target_compile_features(pvz_game_tests PRIVATE cxx_std_23)
    target_link_libraries(
        pvz_game_tests
        PRIVATE
            pvz_game
            pvz_engine_core
            pvz_legacy_profile_io
    )
    add_test(NAME pvz_game_tests COMMAND pvz_game_tests)
    add_test(NAME pvz_game_headless_smoke COMMAND pvz_game_headless)

    add_executable(
        pvz_game_legacy_parity_tests
        tests/LegacyParityTests.cpp
    )
    target_compile_features(
        pvz_game_legacy_parity_tests
        PRIVATE cxx_std_23
    )
    target_link_libraries(
        pvz_game_legacy_parity_tests
        PRIVATE
            pvz_game
            pvz_engine_core
    )
    if(MSVC)
        target_compile_options(
            pvz_game_legacy_parity_tests
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_game_legacy_parity_tests
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
        NAME pvz_game_legacy_parity_tests
        COMMAND pvz_game_legacy_parity_tests
    )
endif()

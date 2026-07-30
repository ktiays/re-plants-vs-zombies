add_library(
    pvz_parity
    STATIC
        src/BehaviorCapture.cpp
        src/BehaviorComparison.cpp
)
target_compile_features(pvz_parity PUBLIC cxx_std_23)
target_include_directories(
    pvz_parity
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/game/include"
)
target_link_libraries(
    pvz_parity
    PUBLIC
        pvz_engine_core
)
add_dependencies(pvz_parity pvz_check_portable_boundaries)

if(MSVC)
    target_compile_options(pvz_parity PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(
        pvz_parity
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Werror
    )
endif()

add_executable(
    pvz_behavior_inspect
    tools/BehaviorInspect.cpp
)
target_compile_features(pvz_behavior_inspect PRIVATE cxx_std_23)
target_link_libraries(pvz_behavior_inspect PRIVATE pvz_parity)

if(MSVC)
    target_compile_options(
        pvz_behavior_inspect
        PRIVATE /W4 /WX /permissive-
    )
else()
    target_compile_options(
        pvz_behavior_inspect
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
        pvz_parity_tests
        tests/BehaviorCaptureTests.cpp
    )
    target_compile_features(pvz_parity_tests PRIVATE cxx_std_23)
    target_link_libraries(pvz_parity_tests PRIVATE pvz_parity)
    if(MSVC)
        target_compile_options(
            pvz_parity_tests
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_parity_tests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Werror
        )
    endif()
    add_test(NAME pvz_parity_tests COMMAND pvz_parity_tests)
endif()

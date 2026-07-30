add_library(
    pvz_win32_reference_input
    STATIC
        src/LegacyInputCapture.cpp
        src/ReferenceInputRecorder.cpp
)
target_compile_features(
    pvz_win32_reference_input
    PUBLIC cxx_std_23
)
target_include_directories(
    pvz_win32_reference_input
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    pvz_win32_reference_input
    PUBLIC
        pvz_engine_core
)
add_dependencies(
    pvz_win32_reference_input
    pvz_check_portable_boundaries
)

if(MSVC)
    target_compile_options(
        pvz_win32_reference_input
        PRIVATE /W4 /WX /permissive-
    )
else()
    target_compile_options(
        pvz_win32_reference_input
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
        pvz_win32_reference_input_tests
        tests/ReferenceInputRecorderTests.cpp
    )
    target_compile_features(
        pvz_win32_reference_input_tests
        PRIVATE cxx_std_23
    )
    target_link_libraries(
        pvz_win32_reference_input_tests
        PRIVATE pvz_win32_reference_input
    )
    if(MSVC)
        target_compile_options(
            pvz_win32_reference_input_tests
            PRIVATE /W4 /WX /permissive-
        )
    else()
        target_compile_options(
            pvz_win32_reference_input_tests
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
        NAME pvz_win32_reference_input_tests
        COMMAND pvz_win32_reference_input_tests
    )
endif()

find_path(
    OpenMPT_INCLUDE_DIR
    NAMES libopenmpt/libopenmpt.h
    HINTS
        /opt/homebrew/opt/libopenmpt
        /usr/local/opt/libopenmpt
    PATH_SUFFIXES include
)

find_library(
    OpenMPT_LIBRARY
    NAMES openmpt
    HINTS
        /opt/homebrew/opt/libopenmpt
        /usr/local/opt/libopenmpt
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    OpenMPT
    REQUIRED_VARS
        OpenMPT_INCLUDE_DIR
        OpenMPT_LIBRARY
)

if(OpenMPT_FOUND AND NOT TARGET OpenMPT::OpenMPT)
    add_library(OpenMPT::OpenMPT UNKNOWN IMPORTED)
    set_target_properties(
        OpenMPT::OpenMPT
        PROPERTIES
            IMPORTED_LOCATION "${OpenMPT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenMPT_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(OpenMPT_INCLUDE_DIR OpenMPT_LIBRARY)

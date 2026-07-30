if(NOT DEFINED PVZ_SOURCE_ROOT)
    message(FATAL_ERROR "PVZ_SOURCE_ROOT is required")
endif()

set(PVZ_PORTABLE_SOURCE_PATTERNS
    "${PVZ_SOURCE_ROOT}/engine/api/*.h"
    "${PVZ_SOURCE_ROOT}/engine/api/*.hpp"
    "${PVZ_SOURCE_ROOT}/engine/api/*.c"
    "${PVZ_SOURCE_ROOT}/engine/api/*.cc"
    "${PVZ_SOURCE_ROOT}/engine/api/*.cpp"
    "${PVZ_SOURCE_ROOT}/engine/audio/*.h"
    "${PVZ_SOURCE_ROOT}/engine/audio/*.hpp"
    "${PVZ_SOURCE_ROOT}/engine/audio/*.c"
    "${PVZ_SOURCE_ROOT}/engine/audio/*.cc"
    "${PVZ_SOURCE_ROOT}/engine/audio/*.cpp"
    "${PVZ_SOURCE_ROOT}/engine/core/*.h"
    "${PVZ_SOURCE_ROOT}/engine/core/*.hpp"
    "${PVZ_SOURCE_ROOT}/engine/core/*.c"
    "${PVZ_SOURCE_ROOT}/engine/core/*.cc"
    "${PVZ_SOURCE_ROOT}/engine/core/*.cpp"
    "${PVZ_SOURCE_ROOT}/engine/image/*.h"
    "${PVZ_SOURCE_ROOT}/engine/image/*.hpp"
    "${PVZ_SOURCE_ROOT}/engine/image/*.c"
    "${PVZ_SOURCE_ROOT}/engine/image/*.cc"
    "${PVZ_SOURCE_ROOT}/engine/image/*.cpp"
    "${PVZ_SOURCE_ROOT}/game/*.h"
    "${PVZ_SOURCE_ROOT}/game/*.hpp"
    "${PVZ_SOURCE_ROOT}/game/*.c"
    "${PVZ_SOURCE_ROOT}/game/*.cc"
    "${PVZ_SOURCE_ROOT}/game/*.cpp"
    "${PVZ_SOURCE_ROOT}/parity/*.h"
    "${PVZ_SOURCE_ROOT}/parity/*.hpp"
    "${PVZ_SOURCE_ROOT}/parity/*.c"
    "${PVZ_SOURCE_ROOT}/parity/*.cc"
    "${PVZ_SOURCE_ROOT}/parity/*.cpp"
    "${PVZ_SOURCE_ROOT}/Lawn/System/DataSync.h"
    "${PVZ_SOURCE_ROOT}/Lawn/System/DataSync.cpp"
    "${PVZ_SOURCE_ROOT}/Lawn/System/LegacySaveFormat.h"
    "${PVZ_SOURCE_ROOT}/Lawn/System/LegacySaveFormat.cpp"
    "${PVZ_SOURCE_ROOT}/Lawn/System/PlayerInfo.h"
    "${PVZ_SOURCE_ROOT}/Lawn/System/PlayerInfoSerialization.cpp"
)

file(
    GLOB_RECURSE PVZ_PORTABLE_SOURCES
    LIST_DIRECTORIES FALSE
    ${PVZ_PORTABLE_SOURCE_PATTERNS}
)

set(PVZ_FORBIDDEN_PATTERNS
    "#[ \t]*include[ \t]*[<\"]windows[.]h[>\"]"
    "#[ \t]*include[ \t]*[<\"]d3d[^>\"]*[>\"]"
    "#[ \t]*include[ \t]*[<\"]ddraw[.]h[>\"]"
    "#[ \t]*include[ \t]*[<\"]dsound[.]h[>\"]"
    "#[ \t]*include[ \t]*[<\"]SDL[^>\"]*[>\"]"
    "#[ \t]*include[ \t]*[<\"]Metal[^>\"]*[>\"]"
    "#[ \t]*include[ \t]*[<\"]AppKit[^>\"]*[>\"]"
    "#[ \t]*include[ \t]*[<\"]Cocoa[^>\"]*[>\"]"
    "(^|[^A-Za-z0-9_])long([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])__int[0-9]+([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(HANDLE|HWND|DWORD|ULONG|WPARAM|LPARAM)([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])(SDL_[A-Za-z0-9_]*|MTL[A-Za-z0-9_]*|IDirect3D[A-Za-z0-9_]*)([^A-Za-z0-9_]|$)"
    "^[ \t]*#[ \t]*(if|ifdef|ifndef).*(_WIN32|__APPLE__|__linux__)"
)

set(PVZ_VIOLATION_COUNT 0)

foreach(PVZ_SOURCE IN LISTS PVZ_PORTABLE_SOURCES)
    file(
        RELATIVE_PATH PVZ_RELATIVE_SOURCE
        "${PVZ_SOURCE_ROOT}"
        "${PVZ_SOURCE}"
    )
    file(STRINGS "${PVZ_SOURCE}" PVZ_LINES)
    set(PVZ_LINE_NUMBER 0)

    foreach(PVZ_LINE IN LISTS PVZ_LINES)
        math(EXPR PVZ_LINE_NUMBER "${PVZ_LINE_NUMBER} + 1")

        if(
            PVZ_RELATIVE_SOURCE MATCHES "^game/(include|src)/" AND
            PVZ_LINE MATCHES
                "#[ \t]*include[ \t]*[<\"]pvz/engine/core/"
        )
            message(
                SEND_ERROR
                "${PVZ_RELATIVE_SOURCE}:${PVZ_LINE_NUMBER}: "
                "portable game may depend only on the engine API: "
                "${PVZ_LINE}"
            )
            math(EXPR PVZ_VIOLATION_COUNT "${PVZ_VIOLATION_COUNT} + 1")
            continue()
        endif()

        foreach(PVZ_PATTERN IN LISTS PVZ_FORBIDDEN_PATTERNS)
            if(PVZ_LINE MATCHES "${PVZ_PATTERN}")
                message(
                    SEND_ERROR
                    "${PVZ_RELATIVE_SOURCE}:${PVZ_LINE_NUMBER}: "
                    "portable-boundary violation: ${PVZ_LINE}"
                )
                math(EXPR PVZ_VIOLATION_COUNT "${PVZ_VIOLATION_COUNT} + 1")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()

if(PVZ_VIOLATION_COUNT GREATER 0)
    message(
        FATAL_ERROR
        "Found ${PVZ_VIOLATION_COUNT} portable-boundary violation(s)"
    )
endif()

message(STATUS "Portable source boundary check passed")

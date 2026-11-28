# RS64AP probe: a log-only native mod for collecting game data (objectives, medals, pickups, hangar masks).
#   cmake -S probes/rs64ap-probe -B build/probe -DCMAKE_BUILD_TYPE=Release && cmake --build build/probe
# The mod folder lands in build/probe/rs64ap-probe/. No dependencies beyond the recomp's host_api.h.
cmake_minimum_required(VERSION 3.20)
project(rs64ap_probe VERSION 0.1.0 LANGUAGES CXX)

set(RS64_RECOMP_DIR "" CACHE PATH "A RogueSquadron64Recomp checkout (for include/rs64/host_api.h). Empty = download it.")
set(RS64_HOST_API_COMMIT 222947ee9844cb2d730b819915660fc8940dd5c3)
if (NOT RS64_RECOMP_DIR)
    set(RS64_RECOMP_DIR "${CMAKE_BINARY_DIR}/_deps/rs64recomp")
    if (NOT EXISTS "${RS64_RECOMP_DIR}/include/rs64/host_api.h")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/MikeSemicolonD/RogueSquadron64Recomp/${RS64_HOST_API_COMMIT}/include/rs64/host_api.h"
            "${RS64_RECOMP_DIR}/include/rs64/host_api.h"
            EXPECTED_HASH SHA256=86dde1b765201d7e1182a2c4e5b7123b3e70af658881dbe3e3deec4a7c285374
            TLS_VERIFY ON)
    endif()
endif()

add_library(rs64ap_probe SHARED src/probe.cpp)
set(PROBE_OUT "${CMAKE_BINARY_DIR}/rs64ap-probe")
set_target_properties(rs64ap_probe PROPERTIES
    PREFIX "" OUTPUT_NAME "rs64ap_probe" CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY "${PROBE_OUT}/$<0:>" RUNTIME_OUTPUT_DIRECTORY "${PROBE_OUT}/$<0:>")
target_include_directories(rs64ap_probe PRIVATE
    "${RS64_RECOMP_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../../client/src")
if (MSVC)
    set_property(TARGET rs64ap_probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif (MINGW)
    target_link_options(rs64ap_probe PRIVATE -static -static-libgcc -static-libstdc++)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(rs64ap_probe PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../../client/exports.map")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/mod.json" "${PROBE_OUT}/mod.json" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${PROBE_OUT}/LICENSE.txt" COPYONLY)

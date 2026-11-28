# RS64AP client: a native mod (shared library) for RogueSquadron64Recomp.
#
#   cmake -S client -B build/client -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/client --config Release
#
# The mod folder lands in build/client/rs64ap/ (library + mod.json + roguesq_menu.json); copy that folder into the
# game's mods/ directory. Dependencies are fetched at configure time. OpenSSL is needed for wss:// (archipelago.gg);
# without it the build still works but can only reach ws:// servers (local hosting).
cmake_minimum_required(VERSION 3.20)
project(rs64ap_client VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

set(RS64_RECOMP_DIR "" CACHE PATH "A RogueSquadron64Recomp checkout (for include/rs64/host_api.h). Empty = fetch the pinned commit.")
option(RS64AP_REQUIRE_SSL "Fail the configure step when OpenSSL is missing" OFF)

# The host API is one self-contained header: download it at the pinned commit (hash-checked) unless a checkout is given.
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

# SOURCE_SUBDIR points at a folder without a CMakeLists.txt, so these are only downloaded, never built.
if (NOT EXISTS "${RS64_RECOMP_DIR}/include/rs64/host_api.h")
    message(FATAL_ERROR "host_api.h not found under ${RS64_RECOMP_DIR}/include/rs64")
endif()

# apclientpp and its header-only dependencies, at the versions apclientpp's own test build uses.
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG f693a3eb7fe72a5f19b975289afc4f437d373d9c
    SOURCE_SUBDIR rs64ap-headers-only)
FetchContent_Declare(websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG 4dfe1be74e684acca19ac1cf96cce0df9eac2a2d
    SOURCE_SUBDIR rs64ap-headers-only)
FetchContent_Declare(wswrap
    GIT_REPOSITORY https://github.com/black-sliver/wswrap.git
    GIT_TAG aeba7ac428028723fb26ce92488f260660f786b1
    GIT_SUBMODULES ""
    SOURCE_SUBDIR rs64ap-headers-only)
FetchContent_Declare(apclientpp
    GIT_REPOSITORY https://github.com/black-sliver/apclientpp.git
    GIT_TAG 6b28cc1781006deae0e28c61752d11a83819585a
    SOURCE_SUBDIR rs64ap-headers-only)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    SOURCE_SUBDIR rs64ap-headers-only)
FetchContent_MakeAvailable(asio websocketpp wswrap apclientpp nlohmann_json)

add_library(rs64ap SHARED
    src/mod.cpp
    src/net.cpp
    src/pilot.cpp)

set(RS64AP_OUT "${CMAKE_BINARY_DIR}/rs64ap")
# $<0:> keeps multi-config generators from adding a per-config folder.
set_target_properties(rs64ap PROPERTIES
    PREFIX ""
    OUTPUT_NAME "rs64ap"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY "${RS64AP_OUT}/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${RS64AP_OUT}/$<0:>")

target_include_directories(rs64ap PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${RS64_RECOMP_DIR}/include"
    "${asio_SOURCE_DIR}/asio/include"
    "${websocketpp_SOURCE_DIR}"
    "${wswrap_SOURCE_DIR}/include"
    "${apclientpp_SOURCE_DIR}"
    "${nlohmann_json_SOURCE_DIR}/include")

target_compile_definitions(rs64ap PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_THREAD_ AP_NO_SCHEMA)

find_package(Threads REQUIRED)
target_link_libraries(rs64ap PRIVATE Threads::Threads)

find_package(OpenSSL)
if (OpenSSL_FOUND)
    target_link_libraries(rs64ap PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    if (RS64AP_REQUIRE_SSL)
        message(FATAL_ERROR "OpenSSL not found (RS64AP_REQUIRE_SSL is ON)")
    endif()
    message(WARNING "OpenSSL not found: building without wss:// support (local ws:// servers only)")
    target_compile_definitions(rs64ap PRIVATE WSWRAP_NO_SSL)
endif()

find_package(ZLIB)
if (ZLIB_FOUND)
    target_link_libraries(rs64ap PRIVATE ZLIB::ZLIB)
else()
    target_compile_definitions(rs64ap PRIVATE WSWRAP_NO_COMPRESSION)
endif()

if (WIN32)
    target_compile_definitions(rs64ap PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(rs64ap PRIVATE ws2_32 mswsock crypt32)
endif()
if (MSVC)
    target_compile_options(rs64ap PRIVATE /Zc:__cplusplus /bigobj /utf-8)
    # Static CRT: the game loads mods with a restricted DLL search that won't find the CRT DLLs.
    set_property(TARGET rs64ap PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif (MINGW)
    target_compile_options(rs64ap PRIVATE -Wall -Wno-deprecated-declarations)
    # Self-contained DLL: no libstdc++/libgcc/winpthread DLLs next to it. OpenSSL's static libs need these too.
    target_link_options(rs64ap PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(rs64ap PRIVATE advapi32 user32 bcrypt)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(rs64ap PRIVATE -Wall -Wno-deprecated-declarations)
    # Export only what the game looks up, and keep the bundled libraries' symbols to ourselves.
    target_link_options(rs64ap PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
        "-Wl,--exclude-libs,ALL" "-Wl,-Bsymbolic" "-Wl,--no-undefined")
endif()

# The rest of the mod folder.
foreach (f mod.json roguesq_menu.json NOTICE.md)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${f}" "${RS64AP_OUT}/${f}" COPYONLY)
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${RS64AP_OUT}/LICENSE.txt" COPYONLY)

# Test harness: loads the built mod with a fake host API and drives it against a real Archipelago server.
option(RS64AP_BUILD_HARNESS "Build the client test harness" OFF)
if (RS64AP_BUILD_HARNESS)
    add_executable(rs64ap_harness tests/harness.cpp)
    set_target_properties(rs64ap_harness PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_include_directories(rs64ap_harness PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${RS64_RECOMP_DIR}/include")
    target_link_libraries(rs64ap_harness PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(rs64ap_harness rs64ap)
    if (MINGW)
        target_link_options(rs64ap_harness PRIVATE -static)
    endif()
endif()

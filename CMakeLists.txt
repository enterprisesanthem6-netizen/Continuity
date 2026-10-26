cmake_minimum_required(VERSION 3.22)
project(ContinuityBE LANGUAGES CXX)

# Native mod (LeviLauncher preload-native) built with the Android NDK toolchain
# file for arm64-v8a, same setup as BetterZoom. Host tests: tests/run_tests.sh.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(continuitybe SHARED
    src/main.cpp
    src/CtmMod.cpp
    src/CtmRules.cpp
    src/GameBridge.cpp
)

target_include_directories(continuitybe PRIVATE include src third_party third_party/fmt)
target_compile_definitions(continuitybe PRIVATE FMT_HEADER_ONLY JSON_NOEXCEPTION)
target_compile_options(continuitybe PRIVATE
    -Os -g0 -w -fno-rtti -fno-exceptions
    -fno-unwind-tables -fno-asynchronous-unwind-tables
    -ffunction-sections -fdata-sections
)

add_library(preloader SHARED IMPORTED)
set_target_properties(preloader PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/libpreloader-1.5.16.so
    IMPORTED_NO_SONAME TRUE
)

target_link_options(continuitybe PRIVATE
    -Wl,-z,max-page-size=16384
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--exclude-libs,ALL
    -Wl,--no-as-needed
)
target_link_libraries(continuitybe PRIVATE preloader log dl m)

string(REGEX REPLACE "(^| )-g[0-9a-z]*( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "(^| )-g[0-9a-z]*( |$)" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

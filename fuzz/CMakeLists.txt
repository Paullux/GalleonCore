# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "GALLEON_FUZZ requires Clang (libFuzzer)")
endif()

# The protocol layer has no Qt or hidapi dependency, so it is compiled
# directly into the fuzzer.
add_executable(fuzz_input_report
    fuzz_input_report.cpp
    ${PROJECT_SOURCE_DIR}/lib/src/Protocol.cpp
)
target_include_directories(fuzz_input_report PRIVATE ${PROJECT_SOURCE_DIR}/lib/include)
target_compile_options(fuzz_input_report PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_input_report PRIVATE -fsanitize=fuzzer,address,undefined)

if(NOT GALLEON_BUILD_DESKTOP)
    return()
endif()

# The pages file parser needs Qt Core (JSON), and the protocol header for the
# key and dial counts.
add_executable(fuzz_pages_config
    fuzz_pages_config.cpp
    ${PROJECT_SOURCE_DIR}/daemon/PagesConfig.cpp
    ${PROJECT_SOURCE_DIR}/daemon/KeyInjector.cpp
    ${PROJECT_SOURCE_DIR}/daemon/DesktopEntries.cpp
    ${PROJECT_SOURCE_DIR}/daemon/WellKnownApps.cpp
)
target_include_directories(fuzz_pages_config PRIVATE ${PROJECT_SOURCE_DIR}/lib/include ${PROJECT_SOURCE_DIR}/daemon)
target_link_libraries(fuzz_pages_config PRIVATE Qt6::Core)
target_compile_options(fuzz_pages_config PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_pages_config PRIVATE -fsanitize=fuzzer,address,undefined)

# The theme file reader: the stored-zip container and the theme validator, which
# runs the pages file parser on the manifest.
add_executable(fuzz_theme_package
    fuzz_theme_package.cpp
    ${PROJECT_SOURCE_DIR}/daemon/StoredZip.cpp
    ${PROJECT_SOURCE_DIR}/daemon/ThemePackage.cpp
    ${PROJECT_SOURCE_DIR}/daemon/PagesConfig.cpp
    ${PROJECT_SOURCE_DIR}/daemon/KeyInjector.cpp
    ${PROJECT_SOURCE_DIR}/daemon/DesktopEntries.cpp
    ${PROJECT_SOURCE_DIR}/daemon/WellKnownApps.cpp
)
target_include_directories(fuzz_theme_package PRIVATE ${PROJECT_SOURCE_DIR}/lib/include ${PROJECT_SOURCE_DIR}/daemon)
target_link_libraries(fuzz_theme_package PRIVATE Qt6::Core)
target_compile_options(fuzz_theme_package PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(fuzz_theme_package PRIVATE -fsanitize=fuzzer,address,undefined)

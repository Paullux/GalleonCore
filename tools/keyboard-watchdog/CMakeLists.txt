# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# The watchdog of the keyboard's live lighting, not built by default
# (-DGALLEON_BUILD_KEYBOARD_WATCHDOG=ON). Its logic (galleon_keyboard: LiveRelease) is compiled and tested with a
# fake keyboard by tests/ on every build; the argument parser and the process wait are tested too. Only main.cpp
# here talks to real hardware.
add_library(keyboard_watchdog_core STATIC WatchdogArgs.cpp ProcessWait.cpp)
target_include_directories(keyboard_watchdog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(keyboard_watchdog_core PRIVATE galleon_warnings)
set_target_properties(keyboard_watchdog_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(galleon-keyboard-watchdog main.cpp)
target_link_libraries(galleon-keyboard-watchdog PRIVATE keyboard_watchdog_core galleon galleon_keyboard galleon_warnings)

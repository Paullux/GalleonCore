# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# Manual test tool, not built by default (-DGALLEON_BUILD_KEYBOARD_BRIGHTNESS_TOOL=ON). Its logic
# (galleon_keyboard) is compiled and tested with a fake transport by tests/ on every build; only
# main.cpp here talks to real hardware.
add_executable(galleon-keyboard-brightness main.cpp)
target_link_libraries(galleon-keyboard-brightness PRIVATE galleon galleon_keyboard galleon_warnings)

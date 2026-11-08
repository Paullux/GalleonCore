# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# Read-only diagnostic: reads the keyboard's firmware and brightness through galleon::HidTransport.
# Not built by default (-DGALLEON_BUILD_KEYBOARD_PROBE=ON). Its logic, KeyboardReadProbe.cpp, is
# compiled and tested with a fake transport by tests/ on every build.
add_executable(galleon-keyboard-read-probe main.cpp KeyboardReadProbe.cpp)
target_link_libraries(galleon-keyboard-read-probe PRIVATE galleon galleon_warnings)

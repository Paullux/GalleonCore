# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# galleon-key-signal: lights ONE key the way another program does (docs/KEY_SIGNALS.md). Built with the desktop (-DGALLEON_BUILD_KEY_SIGNAL_TOOL=OFF to leave it out) and
# installed with the other programs. Its arguments, the sending to the local endpoint (Windows) and the held D-Bus signal (Linux) are a library that tests/ runs against a
# fake galleond; only main.cpp is not tested. It reads no key press and no state of the keyboard.
add_library(key_signal_core STATIC KeySignalArgs.cpp EndpointSend.cpp)
target_include_directories(key_signal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(key_signal_core PUBLIC Qt6::Core Qt6::Network galleon_platform PRIVATE galleon_warnings)
set_target_properties(key_signal_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT WIN32)
    target_sources(key_signal_core PRIVATE HeldSignal.cpp)
    target_link_libraries(key_signal_core PUBLIC Qt6::DBus)
endif()

add_executable(galleon-key-signal main.cpp)
target_link_libraries(galleon-key-signal PRIVATE key_signal_core galleon_warnings)

install(TARGETS galleon-key-signal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

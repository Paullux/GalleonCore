# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: LGPL-3.0-or-later

# Independent of the `galleon` target's Protocol/StreamDeck code: see AGENTS.md "Maintaining
# architecture over time" and docs/ROADMAP.md section 3, "Règles de séparation". Links `galleon`
# only to reuse its Transport/HidTransport headers (PUBLIC, so KeyboardLink.h's own
# <galleon/Transport.h> include resolves for callers of this target too); never includes
# Protocol.h or StreamDeck.h.
add_library(galleon_keyboard STATIC
    include/galleon/keyboard/BragiFrame.h
    include/galleon/keyboard/BragiReply.h
    include/galleon/keyboard/Brightness.h
    include/galleon/keyboard/KeyboardLink.h
    include/galleon/keyboard/LiveCommandGuard.h
    include/galleon/keyboard/LiveFrame.h
    include/galleon/keyboard/LiveLink.h
    include/galleon/keyboard/LivePolicy.h
    include/galleon/keyboard/LiveRelease.h
    include/galleon/keyboard/LiveRequests.h
    include/galleon/keyboard/LiveYield.h
    include/galleon/keyboard/VendorInterface.h
    src/BragiFrame.cpp
    src/BragiReply.cpp
    src/Brightness.cpp
    src/KeyboardLink.cpp
    src/LiveCommandGuard.cpp
    src/LiveFrame.cpp
    src/LiveLink.cpp
    src/LivePolicy.cpp
    src/LiveRelease.cpp
    src/LiveRequests.cpp
    src/Exchange.h
    src/Exchange.cpp
    src/CheckedLink.h
    src/CheckedLink.cpp
    src/LiveYield.cpp
    src/VendorInterface.cpp
)

target_include_directories(galleon_keyboard PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(galleon_keyboard PUBLIC galleon PRIVATE galleon_warnings)
set_target_properties(galleon_keyboard PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Internal static library, like galleon_media/galleon_smtc/galleon_keys/galleon_platform in
# media/CMakeLists.txt: nothing outside this build tree links it yet, so it is never installed
# (an installed .a with no package consumer is what rpmbuild's "unpackaged file" check caught).

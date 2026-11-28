// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// A stand-in for Plasma's PlasmoidItem, for the tests only: Plasma is not installed on the machines that run them. It has the few
// properties the plasmoid sets, and nothing else; the real one is in Plasma and is not tested here.
import QtQuick

Item {
    property Component fullRepresentation
    property Component compactRepresentation
    property Component preferredRepresentation
}

// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Shared widget: falling green letters, "Matrix" style (an easter egg of galleond).
// Rendered off-screen by galleond for the screen and for each key. Keep it free of
// PlasmaCore/Kirigami imports.
//
// Pure function of `tick` and `seed`: no Timer, no state, so galleond sets the frame
// number and renders. Bounded: at most (width / cell) columns, three items each, and a
// fixed set of letters; nothing grows with time.

pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: root

    // The frame number, set by galleond at each step.
    property int tick: 0
    // Makes each surface (the screen, each key) rain differently.
    property int seed: 0
    // Size of one letter cell in pixels, and the length of the fading trail in cells.
    property int cell: 16
    property int trailLength: 9
    // Always ready: nothing to decode.
    readonly property bool ready: true

    readonly property string letters: "ABCDEFGHJKLMNPRSTUVWXYZ0123456789+*<>=:"
    readonly property int columns: Math.max(1, Math.min(64, Math.ceil(width / cell)))
    readonly property int rows: Math.max(1, Math.min(64, Math.ceil(height / cell)))
    // A head falls through rows + trail, then waits a little before it starts again.
    readonly property int cycle: rows + trailLength + 6

    implicitWidth: 720
    implicitHeight: 384
    color: "black"

    function hash(a, b) {
        let h = Math.imul(a + 1, 73856093) ^ Math.imul(b + 1, 19349663) ^ Math.imul(root.seed + 1, 83492791)
        h = Math.imul(h ^ (h >>> 15), 2246822519)
        return (h ^ (h >>> 13)) >>> 0
    }

    // The row of the head of column `c` now (may lie above or below the visible rows).
    function headRow(c) {
        const speed = 0.35 + (hash(c, 1000) % 60) / 100 // cells per tick: 0.35 .. 0.94
        const offset = hash(c, 2000) % root.cycle
        return Math.floor(root.tick * speed + offset) % root.cycle - root.trailLength
    }

    // The letters of a column, one per line; they change slowly, not at each step.
    function columnText(c) {
        const step = Math.floor(root.tick / 3)
        let out = ""
        for (let r = 0; r < root.rows; ++r)
            out += (r > 0 ? "\n" : "") + root.letters.charAt(hash(c * 64 + r, step + (r % 3) * 7) % root.letters.length)
        return out
    }

    Repeater {
        model: root.columns

        Item {
            id: column
            required property int index
            readonly property int head: root.headRow(index)
            readonly property real headFraction: Math.max(0, Math.min(1, (head + 1) * root.cell / root.height))
            readonly property real trailFraction: root.trailLength * root.cell / root.height

            x: index * root.cell
            width: root.cell
            height: root.height
            clip: true

            Text {
                width: root.cell
                height: parent.height
                horizontalAlignment: Text.AlignHCenter
                color: "#00d43a"
                font.family: "monospace"
                font.pixelSize: Math.max(6, root.cell - 3)
                lineHeight: root.cell
                lineHeightMode: Text.FixedHeight
                text: root.columnText(column.index)
            }

            // Hides everything below the head and fades the trail above it.
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: "black" }
                    GradientStop { position: Math.max(0, column.headFraction - column.trailFraction); color: "black" }
                    GradientStop { position: column.headFraction; color: "transparent" }
                    GradientStop { position: Math.min(1, column.headFraction + 0.0001); color: "black" }
                    GradientStop { position: 1; color: "black" }
                }
            }

            // The head of the column, almost white.
            Text {
                visible: column.head >= 0 && column.head < root.rows
                x: 0
                y: column.head * root.cell
                width: root.cell
                height: root.cell
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#d8ffe0"
                font.family: "monospace"
                font.bold: true
                font.pixelSize: Math.max(6, root.cell - 3)
                text: root.letters.charAt(root.hash(column.index, 3000 + Math.floor(root.tick / 2)) % root.letters.length)
            }
        }
    }
}

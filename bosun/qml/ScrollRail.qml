// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// A vertical scroll bar for a Flickable, in plain QML (QtQuick.Controls is not a dependency of this window). It
// shows only when there is something out of sight, follows the Flickable, can be dragged, and a click on the
// track moves by a page. The mouse wheel keeps working on the Flickable itself.
import QtQuick

Item {
    id: rail
    required property Flickable flickable

    readonly property bool needed: flickable.contentHeight > flickable.height + 1
    readonly property real ratio: needed ? Math.min(1, flickable.height / flickable.contentHeight) : 1

    width: 10
    visible: needed
    Accessible.role: Accessible.ScrollBar
    Accessible.name: qsTr("Scroll")

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "#22ffffff"
    }
    // Where the visible part is, and how much of the whole it is.
    Rectangle {
        id: handle
        objectName: "scrollHandle"
        width: parent.width
        height: Math.max(32, rail.height * rail.ratio)
        radius: 5
        y: rail.flickable.contentHeight > rail.flickable.height
           ? (rail.height - height) * rail.flickable.contentY / (rail.flickable.contentHeight - rail.flickable.height)
           : 0
        color: drag.pressed || drag.containsMouse ? "#9aa0a6" : "#6d737c"
    }
    MouseArea {
        id: drag
        anchors.fill: parent
        hoverEnabled: true
        function scrollTo(mouseY) {
            const travel = rail.height - handle.height
            if (travel <= 0)
                return
            const fraction = Math.max(0, Math.min(1, (mouseY - handle.height / 2) / travel))
            rail.flickable.contentY = fraction * (rail.flickable.contentHeight - rail.flickable.height)
        }
        onPressed: mouse => {
            if (mouse.y < handle.y)
                rail.flickable.contentY = Math.max(0, rail.flickable.contentY - rail.flickable.height * 0.9)
            else if (mouse.y > handle.y + handle.height)
                rail.flickable.contentY = Math.min(rail.flickable.contentHeight - rail.flickable.height, rail.flickable.contentY + rail.flickable.height * 0.9)
        }
        onPositionChanged: mouse => { if (pressed && mouse.y >= handle.y && mouse.y <= handle.y + handle.height) scrollTo(mouse.y) }
    }
}

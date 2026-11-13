// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// A scroll bar for a Flickable, vertical or horizontal, in plain QML (QtQuick.Controls is not a dependency of this
// window). It shows only when there is something out of sight, follows the Flickable, can be dragged, and a click on
// the track moves by a page. The mouse wheel and the touchpad keep working on the Flickable itself.
import QtQuick

Item {
    id: rail
    required property Flickable flickable
    property int orientation: Qt.Vertical

    readonly property bool vertical: orientation === Qt.Vertical
    readonly property real viewSize: vertical ? flickable.height : flickable.width
    readonly property real contentSize: vertical ? flickable.contentHeight : flickable.contentWidth
    readonly property real position: vertical ? flickable.contentY : flickable.contentX
    readonly property real trackLength: vertical ? height : width
    readonly property bool needed: contentSize > viewSize + 1
    readonly property real ratio: needed ? Math.min(1, viewSize / contentSize) : 1
    // How far the visible part can move, and where the handle is along the track.
    readonly property real travel: Math.max(0, contentSize - viewSize)
    readonly property real handleLength: Math.max(32, trackLength * ratio)
    readonly property real handleOffset: travel > 0 ? (trackLength - handleLength) * position / travel : 0

    function moveTo(offset) {
        if (vertical)
            flickable.contentY = offset
        else
            flickable.contentX = offset
    }

    implicitWidth: vertical ? 10 : 100
    implicitHeight: vertical ? 100 : 10
    // False when the page the bar belongs to is not shown.
    property bool active: true
    visible: needed && active
    Accessible.role: Accessible.ScrollBar
    Accessible.name: vertical ? qsTr("Scroll vertically") : qsTr("Scroll horizontally")

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "#22ffffff"
    }
    // Where the visible part is, and how much of the whole it is.
    Rectangle {
        id: handle
        objectName: "scrollHandle"
        x: rail.vertical ? 0 : rail.handleOffset
        y: rail.vertical ? rail.handleOffset : 0
        width: rail.vertical ? parent.width : rail.handleLength
        height: rail.vertical ? rail.handleLength : parent.height
        radius: 5
        color: drag.pressed || drag.containsMouse ? "#9aa0a6" : "#6d737c"
    }
    MouseArea {
        id: drag
        anchors.fill: parent
        hoverEnabled: true
        function along(mouse) { return rail.vertical ? mouse.y : mouse.x }
        function scrollTo(point) {
            const room = rail.trackLength - rail.handleLength
            if (room <= 0)
                return
            const fraction = Math.max(0, Math.min(1, (point - rail.handleLength / 2) / room))
            rail.moveTo(fraction * rail.travel)
        }
        onPressed: mouse => {
            const point = along(mouse)
            if (point < rail.handleOffset)
                rail.moveTo(Math.max(0, rail.position - rail.viewSize * 0.9))
            else if (point > rail.handleOffset + rail.handleLength)
                rail.moveTo(Math.min(rail.travel, rail.position + rail.viewSize * 0.9))
        }
        onPositionChanged: mouse => {
            const point = along(mouse)
            if (pressed && point >= rail.handleOffset && point <= rail.handleOffset + rail.handleLength)
                scrollTo(point)
        }
    }
}

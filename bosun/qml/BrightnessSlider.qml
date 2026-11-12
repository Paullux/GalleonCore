// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// A slider of eleven stops, 0 % to 100 % in steps of 10 %, for the keyboard's own brightness. The value is
// in the keyboard's unit (0 to 1000, steps of 100) so it goes to galleond unchanged. Nothing is sent while
// the slider is dragged: `committed` fires once, when the pointer is released or a key press has rested.
// QML only draws and forwards; the write is made by galleond (this application never opens the device).
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: slider

    // The value in force: -1 while unknown (nothing was set from here since Bosun started).
    property int value: -1
    property bool enabledLook: true
    readonly property int stops: 10
    // What the pointer or the keys show now, before it is committed.
    property int preview: -1
    readonly property int shown: preview >= 0 ? preview : value
    signal committed(int value)

    implicitHeight: 86
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Keyboard brightness")
    Accessible.description: slider.shown < 0 ? qsTr("not set from this window yet") : qsTr("%1 %").arg(slider.shown / 10)

    readonly property real trackWidth: width - 2 * handle.width / 2 - 2
    function xFor(stop) { return handle.width / 2 + 1 + stop * trackWidth / stops }
    function stopAt(x) { return Math.max(0, Math.min(stops, Math.round((x - xFor(0)) / trackWidth * stops))) }

    function choose(stop) {
        slider.preview = stop * 100
    }
    function commit() {
        const target = slider.preview
        slider.preview = -1
        if (target >= 0 && target !== slider.value) {
            slider.value = target
            slider.committed(target)
        }
    }

    Timer {
        id: rest
        interval: 450
        onTriggered: slider.commit()
    }

    Keys.onPressed: event => {
        const step = event.key === Qt.Key_Left || event.key === Qt.Key_Down ? -1
                   : event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1 : 0
        if (step === 0 || !slider.enabledLook)
            return
        const from = slider.shown >= 0 ? slider.shown / 100 : 5
        slider.choose(Math.max(0, Math.min(slider.stops, from + step)))
        rest.restart()
        event.accepted = true
    }

    // The figure, big, at the right of the title.
    Text {
        id: figure
        anchors.right: parent.right
        anchors.top: parent.top
        color: slider.enabledLook ? "white" : "#6d737c"
        font.pixelSize: 22
        font.bold: true
        text: slider.shown < 0 ? "—" : slider.shown / 10 + " %"
    }

    Item {
        id: lane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 32
        height: 24

        Rectangle {
            id: groove
            x: slider.xFor(0)
            width: slider.trackWidth
            anchors.verticalCenter: parent.verticalCenter
            height: 8
            radius: 4
            color: "#2e3238"
        }
        Rectangle {
            x: groove.x
            width: slider.shown < 0 ? 0 : slider.xFor(slider.shown / 100) - groove.x
            anchors.verticalCenter: parent.verticalCenter
            height: 8
            radius: 4
            color: slider.enabledLook ? "#2e7d32" : "#44484f"
        }
        Repeater {
            model: slider.stops + 1
            Rectangle {
                required property int index
                x: slider.xFor(index) - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 2
                height: index % 5 === 0 ? 16 : 10
                radius: 1
                color: slider.shown >= 0 && index <= slider.shown / 100 ? "#a5d6a7" : "#4a4f57"
                z: 1
            }
        }
        Rectangle {
            id: handle
            visible: slider.shown >= 0
            x: slider.xFor(slider.shown / 100) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 22
            height: 22
            radius: 11
            color: slider.enabledLook ? "white" : "#8a8f98"
            border.width: slider.activeFocus ? 3 : 2
            border.color: slider.activeFocus ? "#64b5f6" : "#2e7d32"
            z: 2
        }
        MouseArea {
            anchors.fill: parent
            enabled: slider.enabledLook
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: mouse => { slider.forceActiveFocus(); rest.stop(); slider.choose(slider.stopAt(mouse.x)) }
            onPositionChanged: mouse => { if (pressed) slider.choose(slider.stopAt(mouse.x)) }
            onReleased: slider.commit()
            onCanceled: slider.preview = -1
        }
    }

    // 0 %, 10 % ... 100 % under the stops; every other one when the room is short.
    Repeater {
        model: slider.stops + 1
        Text {
            required property int index
            readonly property bool crowded: slider.trackWidth / slider.stops < 40
            visible: !crowded || index % 2 === 0
            x: slider.xFor(index) - width / 2
            y: 62
            color: slider.shown >= 0 && index === slider.shown / 100 ? "white" : "#7d8591"
            font.pixelSize: 12
            text: index * 10 + (index === slider.stops || index === 0 ? " %" : "")
        }
    }
}

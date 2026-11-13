// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// One choice among several, drawn as a radio button: a ring with a dot when chosen, and its label. The parent
// decides what "chosen" means (`checked`) and acts on `chosen`; this only draws and forwards the click or a
// press of Space or Enter.
import QtQuick

Item {
    id: option
    property alias text: label.text
    property bool checked: false
    property bool enabledLook: true
    signal chosen

    implicitWidth: ring.width + 10 + label.implicitWidth
    implicitHeight: 28
    activeFocusOnTab: true
    Accessible.role: Accessible.RadioButton
    Accessible.name: label.text
    Accessible.checked: option.checked

    Keys.onPressed: event => {
        if (option.enabledLook && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            option.chosen()
            event.accepted = true
        }
    }

    Rectangle {
        id: ring
        anchors.verticalCenter: parent.verticalCenter
        width: 18
        height: 18
        radius: 9
        color: "transparent"
        border.width: option.activeFocus ? 3 : 2
        border.color: option.activeFocus ? "#64b5f6" : option.checked ? "#2e7d32" : "#7d8591"
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            color: option.enabledLook ? "#a5d6a7" : "#6d737c"
            visible: option.checked
        }
    }
    Text {
        id: label
        anchors.left: ring.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        color: option.enabledLook ? "white" : "#6d737c"
        font.pixelSize: 14
    }
    MouseArea {
        anchors.fill: parent
        enabled: option.enabledLook
        cursorShape: Qt.PointingHandCursor
        onClicked: { option.forceActiveFocus(); option.chosen() }
    }
}

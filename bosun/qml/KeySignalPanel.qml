// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// "Try a signal": lights ONE key the way another program does (docs/KEY_SIGNALS.md), to see what a program would show. The key is the one chosen on the drawing of the keyboard.
// QML presents and forwards: the request is built and checked in C++ (KeySignalRequest, DaemonStatus), and nothing here reads a key press.
pragma ComponentBehavior: Bound
import QtQuick

Column {
    id: panel

    // DaemonStatus: lightKey / clearKey / clearKeys / keyNameOfLed.
    property var daemon: null
    // The box "Other programs may light keys" (KeySignalsStore.allow): without it galleond refuses.
    property bool allowed: false
    // The LED of the key chosen on the drawing, or -1.
    property int led: -1
    // What the person chose.
    property string colour: "#ff9800"
    property bool pulse: false
    property int seconds: 60
    // The name that would be sent ("F3"), or "" when the chosen place has none.
    readonly property string keyName: panel.daemon !== null && panel.led >= 0 ? panel.daemon.keyNameOfLed(panel.led) : ""
    // The answer of a request: the error ("" when sent) and the message to show when it was sent.
    signal answered(string error, string done)

    spacing: 10
    width: parent ? parent.width : 400

    // galleond's answer to a request comes later (D-Bus on Linux): its refusal is shown in its own words.
    Connections {
        target: panel.daemon
        function onCommandError(text) { if (panel.visible) panel.answered(text, "") }
    }

    Text {
        color: "white"
        font.pixelSize: 15
        font.bold: true
        text: qsTr("Try a signal")
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#c9ced6"
        font.pixelSize: 13
        text: qsTr("Lights one key the way another program would, for the time you choose. It needs the live lighting to be on and the box above to be ticked. Choose the key on the drawing of the keyboard. On Linux the signal goes when this window is closed; on Windows it lasts until its end or until you clear it.")
    }
    Text {
        objectName: "keySignalTarget"
        width: parent.width
        wrapMode: Text.Wrap
        color: "#9aa0a6"
        font.pixelSize: 13
        text: panel.led < 0 ? qsTr("No key chosen: click a key on the drawing above.")
              : panel.keyName === "" ? qsTr("This part has no key whose light was measured: choose another.")
              : qsTr("Key: %1 (its place on a QWERTY keyboard).").arg(panel.keyName)
    }
    Text {
        objectName: "keySignalNeedsBox"
        visible: !panel.allowed
        width: parent.width
        wrapMode: Text.Wrap
        color: "#e0a458"
        font.pixelSize: 13
        text: qsTr("Tick \"Other programs may light keys\" above first: until then nothing is lit.")
    }
    Swatches {
        objectName: "keySignalSwatches"
        color: panel.colour
        onPicked: picked => panel.colour = picked
    }
    Row {
        spacing: 8
        Pill {
            objectName: "keySignalSteady"
            text: qsTr("Steady")
            selected: !panel.pulse
            onClicked: panel.pulse = false
        }
        Pill {
            objectName: "keySignalPulse"
            text: qsTr("Pulse")
            selected: panel.pulse
            onClicked: panel.pulse = true
        }
    }
    Row {
        spacing: 8
        Repeater {
            model: [{"seconds": 10, "label": qsTr("10 s")}, {"seconds": 60, "label": qsTr("1 min")},
                    {"seconds": 600, "label": qsTr("10 min")}, {"seconds": 3600, "label": qsTr("1 h")}]
            delegate: Pill {
                id: duration
                required property var modelData
                objectName: "keySignalDuration" + duration.modelData.seconds
                text: duration.modelData.label
                selected: panel.seconds === duration.modelData.seconds
                onClicked: panel.seconds = duration.modelData.seconds
            }
        }
    }
    Row {
        spacing: 8
        Pill {
            objectName: "keySignalLight"
            enabledLook: panel.daemon !== null && panel.keyName !== ""
            text: qsTr("Light the key")
            onClicked: panel.answered(panel.daemon.lightKey(panel.led, panel.colour, panel.pulse, panel.seconds), qsTr("Asked galleond to light %1.").arg(panel.keyName))
        }
        Pill {
            objectName: "keySignalClear"
            enabledLook: panel.daemon !== null && panel.keyName !== ""
            text: qsTr("Clear this key")
            onClicked: panel.answered(panel.daemon.clearKey(panel.led), qsTr("Asked galleond to take the signal of %1 away.").arg(panel.keyName))
        }
        Pill {
            objectName: "keySignalClearAll"
            enabledLook: panel.daemon !== null
            text: qsTr("Clear all")
            onClicked: panel.answered(panel.daemon.clearKeys(), qsTr("Asked galleond to take every signal of this window away."))
        }
    }
}

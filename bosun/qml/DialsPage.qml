// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The two rings of light around the dials of the module: lit or out, and a level. QML only draws and forwards: the settings are held and written by
// RingStore, and galleond applies them (this application never opens the module).
pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: dialsPage
    property var rings: null // the RingStore
    // What an edit answered: "" when it was done, else why not.
    signal answered(string error)

    Column {
        width: Math.min(parent.width, 640)
        spacing: 14

        Text {
            color: "white"
            font.pixelSize: 18
            font.bold: true
            text: qsTr("Dials")
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            color: "#9aa0a6"
            font.pixelSize: 14
            text: qsTr("The two rings of light around the dials. The left one shows the colour of the theme, the right one is white for the player. When the right dial chooses the wallpaper its ring turns magenta, at the level set here but never under 30 %. With the rings out or at 0 %, nothing lights them.")
        }
        Row {
            spacing: 8
            Pill {
                objectName: "ringsLit"
                text: qsTr("Lit")
                selected: dialsPage.rings !== null && dialsPage.rings.lit
                onClicked: dialsPage.answered(dialsPage.rings.setLit(true))
            }
            Pill {
                objectName: "ringsOut"
                text: qsTr("Out")
                selected: dialsPage.rings !== null && !dialsPage.rings.lit
                onClicked: dialsPage.answered(dialsPage.rings.setLit(false))
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            color: "#9aa0a6"
            font.pixelSize: 14
            text: qsTr("Level of the rings: how strongly the colours are shown.")
        }
        BrightnessSlider {
            id: levelSlider
            objectName: "ringsLevel"
            width: Math.min(parent.width, 560)
            accessibleName: qsTr("Level of the rings")
            enabledLook: dialsPage.rings !== null && dialsPage.rings.lit
            value: dialsPage.rings !== null ? dialsPage.rings.level : 1000
            // The slider sets its own value when it commits, which cuts the binding: follow the settings again after any change.
            Connections {
                target: dialsPage.rings
                function onChanged() { if (dialsPage.rings !== null) levelSlider.value = dialsPage.rings.level }
            }
            onCommitted: level => dialsPage.answered(dialsPage.rings.setLevel(level))
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            visible: dialsPage.rings !== null && !dialsPage.rings.lit
            color: "#ffb74d"
            font.pixelSize: 14
            text: qsTr("The rings are out. They light up again for the wallpaper choice, then go out.")
        }
    }
}

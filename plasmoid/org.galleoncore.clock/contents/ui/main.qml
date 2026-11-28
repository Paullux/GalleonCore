// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.plasma.plasmoid
import "WidgetPaths.js" as Paths

// Shows the widget that galleond draws on the Galleon's screen (widgets/WallpaperClock.qml of the project, installed in
// share/galleoncore/widgets), as it is, on the desktop. The widget is shared and never copied here: it is looked for in the
// installed folders (WidgetPaths.js), each one tried in turn until one loads. For now it shows a black screen with the time,
// following the clock of the computer; the wallpaper and the state of the module (a track, a theme) will come from galleond
// over D-Bus (org.galleoncore.Deck), which is not done yet.
PlasmoidItem {
    id: root

    // Folders tried before the installed ones (the tests, a source tree). None by default.
    property var extraFolders: []
    readonly property var folders: Paths.candidates(extraFolders)

    // The widget of the first folder that has one, as a loaded component; null when none has. Local files are read at once, so this
    // is known before anything is shown. (A Loader that goes from one missing file to another stays in the same error state and
    // says nothing, so it cannot be left to try the folders one by one.)
    readonly property var widgetComponent: {
        for (const folder of folders) {
            const component = Qt.createComponent(Paths.widgetUrl(folder, "WallpaperClock.qml"), Component.PreferSynchronous);
            if (component.status === Component.Ready)
                return component;
        }
        return null;
    }

    preferredRepresentation: fullRepresentation

    fullRepresentation: Item {
        implicitWidth: 360
        implicitHeight: 192 // the ratio of the screen of the module, 720 x 384

        Loader {
            objectName: "widget"
            anchors.fill: parent
            sourceComponent: root.widgetComponent
            onLoaded: item.live = true
        }

        Text {
            objectName: "notFound"
            anchors.centerIn: parent
            visible: root.widgetComponent === null
            color: palette.text
            horizontalAlignment: Text.AlignHCenter
            width: parent.width - 20
            wrapMode: Text.WordWrap
            text: "GalleonCore is not installed: its widgets were not found in " + root.folders.join(", ") + "."
        }
    }
}

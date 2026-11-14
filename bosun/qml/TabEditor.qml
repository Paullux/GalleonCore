// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The tab of a theme: its name, its colour, its place among the tabs, and its removal.
pragma ComponentBehavior: Bound

import QtQuick

Column {
    id: editor

    required property var backend
    required property int index // the theme's place in the list
    required property var theme // {name, page, color}
    property var filePicker: null
    property var library: null // the wallpapers of the catalog: where the thumbnail of the tab's own comes from
    property string author: ""
    property string license: "CC-BY-SA-4.0"
    property string description: ""
    property bool includeDials: false
    property var wallpapers: [] // picture files to put in the theme (at most 4)
    property bool confirming: false
    signal answered(string error, string done)
    // The tab moved or went: the window follows it.
    signal moved(int to)
    signal removed

    spacing: 10
    onIndexChanged: wallpapers = []

    Timer {
        id: forget
        interval: 4000
        onTriggered: editor.confirming = false
    }

    Text {
        color: "white"
        font.pixelSize: 16
        font.bold: true
        text: qsTr("This tab")
    }
    Field {
        width: 260
        maximumLength: 24
        bold: true
        placeholder: qsTr("Name of the tab")
        value: editor.theme.name
        onEdited: text => editor.answered(editor.backend.setTheme(editor.index, text, editor.theme.color), qsTr("Name saved."))
    }
    Swatches {
        color: editor.theme.color
        onPicked: c => editor.answered(editor.backend.setTheme(editor.index, editor.theme.name, c), qsTr("Colour saved."))
    }
    Row {
        spacing: 8
        Pill {
            text: qsTr("← Move left")
            enabledLook: editor.index > 0
            onClicked: {
                const error = editor.backend.moveTheme(editor.index, -1)
                editor.answered(error, qsTr("Moved."))
                if (error === "")
                    editor.moved(editor.index - 1)
            }
        }
        Pill {
            text: qsTr("Move right →")
            enabledLook: editor.index < editor.backend.themes.length - 1
            onClicked: {
                const error = editor.backend.moveTheme(editor.index, 1)
                editor.answered(error, qsTr("Moved."))
                if (error === "")
                    editor.moved(editor.index + 1)
            }
        }
        Pill {
            danger: true
            text: editor.confirming ? qsTr("Click again to remove") : qsTr("Remove this tab")
            onClicked: {
                if (!editor.confirming) {
                    editor.confirming = true
                    forget.restart()
                    return
                }
                editor.confirming = false
                const error = editor.backend.removeTheme(editor.index)
                editor.answered(error, qsTr("Tab removed."))
                if (error === "")
                    editor.removed()
            }
        }
    }
    // A second page for this tab: "<tab>-plus", reached from its last free key.
    Row {
        visible: editor.backend.subPagesOf(editor.theme.page).length === 0
        spacing: 10
        Pill {
            text: qsTr("Add a “plus” page")
            onClicked: editor.answered(editor.backend.addSubPage(editor.theme.page), qsTr("Page created: its key is the last free one of this page."))
        }
        Text {
            height: 32
            verticalAlignment: Text.AlignVCenter
            color: "#7d8591"
            font.pixelSize: 12
            text: qsTr("12 more keys, reached from a “More” key.")
        }
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#7d8591"
        font.pixelSize: 12
        text: qsTr("Page “%1”. Removing a tab removes its page too, unless a key still leads to it. The tab that opens the home page cannot be removed.").arg(editor.theme.page)
    }
    Rectangle {
        width: parent.width
        height: 1
        color: "#33ffffff"
    }
    Text {
        color: "white"
        font.pixelSize: 16
        font.bold: true
        text: qsTr("Wallpaper of this tab")
    }
    Text {
        objectName: "tabWallpaperState"
        width: parent.width
        wrapMode: Text.Wrap
        color: "#9aa0a6"
        font.pixelSize: 13
        text: editor.theme.wallpaper === ""
              ? qsTr("None: this tab shows your own wallpaper.")
              : qsTr("This tab shows “%1” on the screen.").arg(editor.theme.wallpaper.split("/").pop())
    }
    // What the tab puts on the screen, as a thumbnail (the same one the Wallpapers tab shows for it).
    Image {
        id: tabWallpaperThumbnail
        objectName: "tabWallpaperThumbnail"
        visible: editor.theme.wallpaper !== ""
        width: Math.min(parent.width, 320)
        height: width * 384 / 720 // the screen's own shape
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        sourceSize.width: 320
        sourceSize.height: 171
        source: editor.theme.wallpaper === "" ? "" : "image://wallpapers/" + editor.theme.wallpaper
        // The picture of the tab is saved under the same name when it is replaced: ask again for it when the pages change.
        readonly property int revision: editor.backend.revision
        onRevisionChanged: {
            if (editor.library)
                editor.library.reload()
            const current = source
            source = ""
            source = current
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: "#33ffffff"
            border.width: 1
            radius: 4
        }
    }
    Row {
        spacing: 8
        Pill {
            objectName: "chooseTabWallpaper"
            text: qsTr("Choose a picture…")
            enabledLook: editor.filePicker !== null
            onClicked: {
                const path = editor.filePicker.picture()
                if (path !== "")
                    editor.answered(editor.backend.setThemeWallpaper(editor.index, path), qsTr("Wallpaper of the tab saved."))
            }
        }
        Pill {
            objectName: "clearTabWallpaper"
            text: qsTr("Use my own wallpaper")
            enabledLook: editor.theme.wallpaper !== ""
            onClicked: editor.answered(editor.backend.clearThemeWallpaper(editor.index), qsTr("The tab uses your own wallpaper again."))
        }
    }
    Rectangle {
        width: parent.width
        height: 1
        color: "#33ffffff"
    }
    Text {
        color: "white"
        font.pixelSize: 16
        font.bold: true
        text: qsTr("Share this tab")
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#7d8591"
        font.pixelSize: 12
        text: qsTr("Makes a .gctheme file with this tab's keys and pictures. Share only what you made or may redistribute: no logos, characters or screenshots of others.")
    }
    Row {
        spacing: 8
        Field {
            width: 180
            maximumLength: 64
            placeholder: qsTr("Author")
            value: editor.author
            input.onTextChanged: editor.author = input.text
        }
        Field {
            width: 140
            maximumLength: 40
            placeholder: qsTr("Licence")
            value: editor.license
            input.onTextChanged: editor.license = input.text
        }
    }
    Field {
        width: 328
        maximumLength: 300
        placeholder: qsTr("A line about the theme (optional)")
        value: editor.description
        input.onTextChanged: editor.description = input.text
    }
    Row {
        spacing: 8
        Pill {
            objectName: "addThemeWallpapers"
            text: qsTr("Add wallpapers…")
            enabledLook: editor.filePicker !== null && editor.wallpapers.length < 4
            onClicked: {
                const chosen = editor.filePicker.pictures()
                const merged = editor.wallpapers.slice()
                for (const path of chosen) {
                    if (merged.length < 4 && merged.indexOf(path) < 0)
                        merged.push(path)
                }
                editor.wallpapers = merged
            }
        }
        Pill {
            visible: editor.wallpapers.length > 0
            text: qsTr("Remove them")
            onClicked: editor.wallpapers = []
        }
        Text {
            visible: editor.wallpapers.length > 0
            height: 32
            verticalAlignment: Text.AlignVCenter
            color: "#9aa0a6"
            font.pixelSize: 12
            text: qsTr("Wallpapers: %1").arg(editor.wallpapers.map(p => p.split("/").pop()).join(", "))
        }
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#7d8591"
        font.pixelSize: 12
        text: qsTr("Up to 4 wallpapers: they are shrunk to the screen and saved as JPEG.")
    }
    Row {
        spacing: 8
        Pill {
            objectName: "exportTheme"
            selected: true
            text: qsTr("Export as a theme file…")
            enabledLook: editor.filePicker !== null && editor.author.trim() !== "" && editor.license.trim() !== ""
            onClicked: {
                const path = editor.filePicker.themeSave(editor.theme.page)
                if (path === "")
                    return
                const result = editor.backend.exportTheme(editor.index, path, {
                    author: editor.author.trim(), license: editor.license.trim(),
                    description: editor.description.trim(), includeDials: editor.includeDials,
                    wallpapers: editor.wallpapers})
                if (result.ok) {
                    let words = qsTr("Theme saved.")
                    if (result.warnings.length > 0)
                        words += " " + result.warnings.join(" · ")
                    editor.answered("", words)
                } else {
                    editor.answered(result.errors.slice(0, 3).join(" · "), "")
                }
            }
        }
        Pill {
            text: editor.includeDials ? qsTr("✓ With my dial settings") : qsTr("With my dial settings")
            selected: editor.includeDials
            onClicked: editor.includeDials = !editor.includeDials
        }
    }
}

// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// A new tab: a name and a colour. It starts with 12 empty keys.
import QtQuick

Column {
    id: form

    required property var backend
    property var filePicker: null
    property bool useDials: false
    property string name: ""
    property string color: "#00acc1"
    property bool withPlus: false
    signal answered(string error, string done)
    // The tab was made: the window shows it.
    signal created

    // Turns what importTheme answered into the words of the status bar.
    function importAnswered(result) {
        if (result.ok) {
            let words = qsTr("Theme installed as a new tab.")
            if (result.warnings.length > 0)
                words += " " + result.warnings.join(" · ")
            form.answered("", words)
        } else {
            form.answered(result.errors.slice(0, 3).join(" · "), "")
        }
    }

    spacing: 12

    Text {
        color: "white"
        font.pixelSize: 18
        font.bold: true
        text: qsTr("A new tab")
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#9aa0a6"
        font.pixelSize: 14
        text: qsTr("A tab is a theme: its own page of 12 keys, and its colour on the module's screen, rings and keys. Up to 8.")
    }
    Field {
        width: 260
        maximumLength: 24
        placeholder: qsTr("Name of the tab")
        value: form.name
        input.onTextChanged: form.name = input.text
    }
    Swatches {
        color: form.color
        onPicked: c => form.color = c
    }
    Row {
        spacing: 10
        Pill {
            text: form.withPlus ? qsTr("✓ With a “plus” page") : qsTr("With a “plus” page")
            selected: form.withPlus
            onClicked: form.withPlus = !form.withPlus
        }
        Text {
            height: 32
            verticalAlignment: Text.AlignVCenter
            color: "#7d8591"
            font.pixelSize: 12
            text: qsTr("12 more keys: the last key of the tab leads to a second page.")
        }
    }
    Pill {
        selected: true
        text: qsTr("Create the tab")
        enabledLook: form.name.trim() !== ""
        onClicked: {
            let error = form.backend.addTheme(form.name, form.color)
            if (error === "" && form.withPlus)
                error = form.backend.addSubPage(form.backend.themes[form.backend.themes.length - 1].page)
            form.answered(error, qsTr("Tab created."))
            if (error === "") {
                form.name = ""
                form.created()
            }
        }
    }
    Rectangle {
        width: parent.width
        height: 1
        color: "#33ffffff"
    }
    Text {
        color: "white"
        font.pixelSize: 18
        font.bold: true
        text: qsTr("Or install a theme")
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#9aa0a6"
        font.pixelSize: 14
        text: qsTr("A theme file (.gc.theme) adds a tab with its keys. It is checked completely first; nothing of yours is replaced. Only install themes from people you trust.")
    }
    Row {
        spacing: 10
        Pill {
            objectName: "importTheme"
            selected: true
            text: qsTr("Install a theme file…")
            enabledLook: form.filePicker !== null
            onClicked: {
                const path = form.filePicker.theme()
                if (path !== "")
                    form.importAnswered(form.backend.importTheme(path, form.useDials))
            }
        }
        Pill {
            objectName: "importThemeDials"
            text: form.useDials ? qsTr("✓ Use its dial settings too") : qsTr("Use its dial settings too")
            selected: form.useDials
            onClicked: form.useDials = !form.useDials
        }
    }
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#7d8591"
        font.pixelSize: 12
        text: qsTr("The two dials are shared by every tab: a theme changes them only if you ask.")
    }
}

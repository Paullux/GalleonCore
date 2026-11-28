// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The profile (a complete pages file, see daemon/Profiles.h) that this window edits and the module shows: chosen at the top of the window, next to the language.
// The list is the profiles of the folder, the active one ticked; below it, a new profile (a copy of the one in use), and renaming or deleting the one in use.
// Renaming or deleting the profile in use first goes back to "default" (and asks galleond to show it), so that galleond never has its file taken away.
// A deleted profile is moved to profiles/.deleted, never erased. Nothing here reads the active window or any key press: the person chooses.
pragma ComponentBehavior: Bound

import QtQuick

Rectangle {
    id: menu
    objectName: "profileMenu"

    required property var backend
    required property var daemon
    property bool expanded: false
    // The question being asked: "", "create", "rename" or "delete". `reason` is why the last answer was refused, shown in the question.
    property string dialog: ""
    property string reason: ""
    // A word for the status bar of the window.
    signal said(string text)

    readonly property string active: menu.backend.activeProfile
    readonly property bool onDefault: menu.active === "default"

    // Shows this profile: the window edits it from now on (the choice is remembered), then galleond is asked to show it on the module.
    function switchTo(name) {
        const answer = menu.backend.setProfile(name)
        if (!answer.ok) {
            menu.said(answer.error)
            return false
        }
        const asked = menu.daemon.setProfile(name)
        if (asked !== "")
            menu.said(asked)
        return true
    }
    function choose(name) {
        menu.expanded = false
        if (name !== menu.active)
            menu.switchTo(name)
    }
    function ask(kind) {
        menu.expanded = false
        menu.reason = ""
        nameField.input.text = ""
        menu.dialog = kind
        if (kind !== "delete")
            nameField.input.forceActiveFocus()
    }
    function cancel() {
        menu.dialog = ""
        menu.reason = ""
    }
    // The answer of the question: `typed` is the name the person wrote (for a new profile, or the new name of the one renamed).
    function confirm(typed) {
        const name = typed.trim()
        const old = menu.active
        if (menu.dialog === "create") {
            const answer = menu.backend.createProfile(name, old)
            if (!answer.ok) {
                menu.reason = answer.error
                return
            }
            menu.cancel()
            if (menu.switchTo(name))
                menu.said(qsTr("Profile created: %1").arg(name))
        } else if (menu.dialog === "rename" && old !== "default") {
            menu.switchTo("default")
            const answer = menu.backend.renameProfile(old, name)
            if (!answer.ok) {
                menu.reason = answer.error
                menu.switchTo(old) // nothing was changed: back where the person was
                return
            }
            menu.cancel()
            menu.switchTo(name)
            menu.said(qsTr("Profile renamed: %1").arg(name))
        } else if (menu.dialog === "delete" && old !== "default") {
            menu.switchTo("default")
            const answer = menu.backend.deleteProfile(old)
            if (!answer.ok) {
                menu.reason = answer.error
                menu.switchTo(old)
                return
            }
            menu.cancel()
            menu.said(qsTr("Profile deleted: %1").arg(old))
        }
    }

    visible: menu.backend.profilesAvailable
    width: 176
    height: 32
    radius: 6
    color: "#2e3238"
    activeFocusOnTab: true
    border.width: activeFocus ? 2 : 0
    border.color: "#80e5d1"
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            menu.expanded = !menu.expanded
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            menu.expanded = false
            menu.cancel()
            event.accepted = true
        }
    }
    Accessible.role: Accessible.ComboBox
    Accessible.name: qsTr("Profile")
    Accessible.onPressAction: { menu.forceActiveFocus(); menu.expanded = !menu.expanded }

    Text {
        anchors.centerIn: parent
        width: parent.width - 16
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: "white"
        font.pixelSize: 12
        text: qsTr("Profile: %1").arg(menu.active) + " ▾"
    }
    MouseArea {
        anchors.fill: parent
        onClicked: { menu.forceActiveFocus(); menu.expanded = !menu.expanded }
    }

    // The list, then what can be done with a profile.
    Column {
        anchors.top: parent.bottom
        anchors.right: parent.right
        visible: menu.expanded
        z: 100
        Repeater {
            model: menu.backend.profiles
            delegate: Rectangle {
                id: option
                required property string modelData
                objectName: "profileOption_" + modelData
                width: 220
                height: 36
                color: activeFocus ? "#3a3f47" : "#2e3238"
                activeFocusOnTab: visible
                Keys.onReturnPressed: menu.choose(option.modelData)
                Keys.onSpacePressed: menu.choose(option.modelData)
                Keys.onEscapePressed: { menu.expanded = false; menu.forceActiveFocus() }
                Accessible.role: Accessible.RadioButton
                Accessible.checkable: true
                Accessible.checked: option.modelData === menu.active
                Accessible.name: option.modelData
                Accessible.onPressAction: menu.choose(option.modelData)
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    color: "white"
                    font.pixelSize: 14
                    text: (option.modelData === menu.active ? "✓ " : "") + option.modelData
                }
                MouseArea { anchors.fill: parent; onClicked: menu.choose(option.modelData) }
            }
        }
        Repeater {
            model: [
                {kind: "create", name: qsTr("New profile (a copy of this one)…"), enabled: menu.backend.profiles.length < 16},
                {kind: "rename", name: qsTr("Rename this profile…"), enabled: !menu.onDefault},
                {kind: "delete", name: qsTr("Delete this profile…"), enabled: !menu.onDefault}
            ]
            delegate: Rectangle {
                id: action
                required property var modelData
                objectName: "profileAction_" + modelData.kind
                width: 220
                height: 36
                color: activeFocus ? "#3a3f47" : "#262a2f"
                opacity: action.modelData.enabled ? 1 : 0.45
                activeFocusOnTab: visible && action.modelData.enabled
                Keys.onReturnPressed: if (action.modelData.enabled) menu.ask(action.modelData.kind)
                Keys.onSpacePressed: if (action.modelData.enabled) menu.ask(action.modelData.kind)
                Accessible.role: Accessible.Button
                Accessible.name: action.modelData.name
                Accessible.onPressAction: if (action.modelData.enabled) menu.ask(action.modelData.kind)
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    color: action.modelData.kind === "delete" ? "#ef9a9a" : "#cfd4da"
                    font.pixelSize: 13
                    text: action.modelData.name
                }
                MouseArea { anchors.fill: parent; enabled: action.modelData.enabled; onClicked: menu.ask(action.modelData.kind) }
            }
        }
    }

    // The question: a name for a new or renamed profile, a confirmation for a deleted one.
    Rectangle {
        id: question
        objectName: "profileDialog"
        visible: menu.dialog !== ""
        anchors.top: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: 4
        width: 330
        height: questionColumn.implicitHeight + 24
        radius: 8
        color: "#23262b"
        border.width: 1
        border.color: "#55ffffff"
        z: 101
        Column {
            id: questionColumn
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 10
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: "white"
                font.pixelSize: 14
                text: menu.dialog === "create" ? qsTr("Name of the new profile")
                    : menu.dialog === "rename" ? qsTr("New name of the profile “%1”").arg(menu.active)
                    : qsTr("Delete the profile “%1”? It is moved to profiles/.deleted, not erased.").arg(menu.active)
            }
            Field {
                id: nameField
                objectName: "profileNameField"
                visible: menu.dialog === "create" || menu.dialog === "rename"
                width: parent.width
                maximumLength: 32
                placeholder: qsTr("1 to 32 letters (a to z), digits or dashes")
            }
            // Enter answers the question. (Not the end of the editing of the field: leaving it to press Cancel must not answer it.)
            Connections {
                target: nameField.input
                function onAccepted() { menu.confirm(nameField.input.text) }
            }
            Text {
                visible: menu.reason !== ""
                width: parent.width
                wrapMode: Text.Wrap
                color: "#ef9a9a"
                font.pixelSize: 13
                text: menu.reason
            }
            Row {
                spacing: 10
                Pill {
                    objectName: "profileDialogOk"
                    selected: menu.dialog !== "delete"
                    danger: menu.dialog === "delete"
                    text: menu.dialog === "create" ? qsTr("Create") : menu.dialog === "rename" ? qsTr("Rename") : qsTr("Delete")
                    onClicked: menu.confirm(menu.dialog === "delete" ? "" : nameField.input.text)
                }
                Pill {
                    objectName: "profileDialogCancel"
                    text: qsTr("Cancel")
                    onClicked: menu.cancel()
                }
            }
        }
    }
}

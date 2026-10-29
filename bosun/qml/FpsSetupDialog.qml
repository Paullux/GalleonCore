// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
Window {
    id: dialog
    objectName: "fpsSetupDialog"
    required property var setup
    property var selectedIds: ({})
    property bool scanning: false
    property bool showResults: false
    readonly property int selectionCount: Object.keys(selectedIds).length
    title: qsTr("Configurer les FPS sous Linux")
    flags: Qt.Dialog
    modality: Qt.WindowModal
    width: 820; height: 680
    minimumWidth: 560; minimumHeight: 420
    color: "#171d26"
    onClosing: close => { if (setup.busy) close.accepted = false }
    function refresh() {
        if (setup.busy) return
        scanning = true
        setup.scan()
    }
    function open() { visible = true; refresh(); requestActivate() }
    function groups() {
        let names = []
        for (let row of setup.rows) if (names.indexOf(row.group) < 0) names.push(row.group)
        return names
    }
    function groupRows(group) { return setup.rows.filter(row => row.group === group) }
    function toggle(id, enabled) {
        let selection = Object.assign({}, selectedIds)
        if (enabled) selection[id] = true
        else delete selection[id]
        selectedIds = selection
    }
    function selectAll(enabled) {
        let selection = {}
        if (enabled) for (let row of setup.rows) if (row.enabled) selection[row.id] = true
        selectedIds = selection
    }
    function toggleGroup(group) {
        const rows = groupRows(group).filter(row => row.enabled)
        const all = rows.length > 0 && rows.every(row => !!selectedIds[row.id])
        let selection = Object.assign({}, selectedIds)
        for (let row of rows) {
            if (all) delete selection[row.id]
            else selection[row.id] = true
        }
        selectedIds = selection
    }
    Connections {
        target: dialog.setup
        function onChanged() {
            if (!dialog.setup.busy) {
                dialog.scanning = false
                let selection = {}
                for (let row of dialog.setup.rows)
                    if (dialog.selectedIds[row.id] && row.enabled) selection[row.id] = true
                dialog.selectedIds = selection
            }
        }
    }
    component ActionButton: Pill {
        activeFocusOnTab: enabled
        enabledLook: enabled
        border.width: activeFocus ? 1 : 0
        border.color: "#80e5d1"
        Keys.onSpacePressed: if (enabled) clicked()
        Keys.onReturnPressed: if (enabled) clicked()
        Accessible.onPressAction: if (enabled) clicked()
    }
    component Choice: Rectangle {
        id: choice
        property string label: ""
        property int state: 0
        property bool available: true
        signal toggled()
        height: Math.max(36, choiceText.implicitHeight + 10)
        color: focus ? "#303c4d" : "transparent"
        opacity: available ? 1 : 0.5
        activeFocusOnTab: available
        Accessible.role: Accessible.CheckBox
        Accessible.name: label
        Accessible.checkable: true
        Accessible.checked: state === 2
        Accessible.onPressAction: if (available) toggled()
        Keys.onSpacePressed: if (available) toggled()
        Rectangle {
            x: 2; y: 8; width: 20; height: 20; radius: 3
            color: choice.state > 0 ? "#80e5d1" : "#202a38"
            border.color: "#80e5d1"
            Text {
            textFormat: Text.PlainText; anchors.centerIn: parent; text: choice.state === 2 ? "✓" : choice.state === 1 ? "−" : ""; color: "#101820" }
        }
        Text {
            textFormat: Text.PlainText;
            id: choiceText
            x: 32; y: 6; width: parent.width - 38
            text: choice.label; color: "white"; wrapMode: Text.Wrap; font.pixelSize: 15
        }
        MouseArea { anchors.fill: parent; enabled: choice.available; onClicked: { choice.forceActiveFocus(); choice.toggled() } }
    }
    Column {
        id: heading
        x: 20; y: 16; width: parent.width - 40; spacing: 10
        Text {
            textFormat: Text.PlainText; text: qsTr("Configuration des jeux et des lanceurs"); color: "white"; font.pixelSize: 21; font.bold: true }
        Text {
            textFormat: Text.PlainText;
            width: parent.width; wrapMode: Text.Wrap; color: "#c9ced6"
            text: qsTr("Fermez les lanceurs avant d’appliquer. Sélectionnez la configuration commune une première fois, puis les jeux à préparer. Les options Steam déjà remplies sont conservées. Les autres modifications sont sauvegardées et peuvent être restaurées depuis le compte rendu.")
        }
        Text {
            textFormat: Text.PlainText;
            width: parent.width; wrapMode: Text.Wrap; color: "#c9ced6"
            text: qsTr("Heroic : choisissez séparément les futurs jeux et les jeux déjà installés. Bottles : le réglage concerne toute la bouteille. Les extensions MangoHud Flatpak restent à installer pour le runtime de chaque lanceur.")
        }
        Row {
            spacing: 8
            ActionButton { objectName: "fpsSelectAll"; text: qsTr("Tout sélectionner"); enabled: !dialog.setup.busy; onClicked: dialog.selectAll(true) }
            ActionButton { objectName: "fpsSelectNone"; text: qsTr("Tout désélectionner"); enabled: !dialog.setup.busy; onClicked: dialog.selectAll(false) }
            ActionButton { text: qsTr("Actualiser"); enabled: !dialog.setup.busy; onClicked: dialog.refresh() }
        }
        Row {
            spacing: 10
            ActionButton { text: qsTr("Sélection"); selected: !dialog.showResults; onClicked: dialog.showResults = false }
            ActionButton { text: qsTr("Compte rendu (%1)").arg(dialog.setup.results.length); selected: dialog.showResults; onClicked: dialog.showResults = true }
        }
    }
    Flickable {
        id: scroll
        x: 20; y: heading.y + heading.height + 12
        width: parent.width - 40; height: Math.max(0, footer.y - y - 12)
        clip: true; contentWidth: width
        contentHeight: dialog.showResults ? report.height : choices.height
        Column {
            id: choices
            width: scroll.width; spacing: 12; visible: !dialog.showResults
            Text {
            textFormat: Text.PlainText;
                visible: dialog.setup.rows.length === 0; width: parent.width; wrapMode: Text.Wrap
                color: "#c9ced6"; text: dialog.setup.busy ? qsTr("Détection en cours…") : qsTr("Aucun élément détecté. Actualisez pour réessayer.")
            }
            Repeater {
                model: dialog.groups()
                delegate: Column {
                    id: group
                    required property string modelData
                    width: choices.width; spacing: 3
                    readonly property var rows: dialog.groupRows(modelData)
                    readonly property var enabledRows: rows.filter(row => row.enabled)
                    readonly property int selectedCount: enabledRows.filter(row => !!dialog.selectedIds[row.id]).length
                    Choice {
                        width: parent.width; label: group.modelData
                        state: group.selectedCount === 0 ? 0 : group.selectedCount === group.enabledRows.length ? 2 : 1
                        available: !dialog.setup.busy && group.enabledRows.length > 0
                        onToggled: dialog.toggleGroup(group.modelData)
                    }
                    Repeater {
                        model: group.rows
                        delegate: Column {
                            id: entry
                            required property var modelData
                            x: 24; width: group.width - 24
                            Choice {
                                width: parent.width; label: entry.modelData.label
                                state: dialog.selectedIds[entry.modelData.id] ? 2 : 0
                                available: !dialog.setup.busy && entry.modelData.enabled
                                onToggled: dialog.toggle(entry.modelData.id, !dialog.selectedIds[entry.modelData.id])
                            }
                            Text {
            textFormat: Text.PlainText;
                                x: 32; width: parent.width - 38
                                visible: text.length > 0; text: entry.modelData.note
                                wrapMode: Text.Wrap; color: entry.modelData.enabled ? "#aebac6" : "#ffb74d"; font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }
        Column {
            id: report
            width: scroll.width; spacing: 14; visible: dialog.showResults
            Text {
            textFormat: Text.PlainText; visible: dialog.setup.results.length === 0; text: qsTr("Aucune application effectuée."); color: "#c9ced6" }
            Repeater {
                model: dialog.setup.results
                delegate: Column {
                    id: result
                    required property var modelData
                    width: report.width; spacing: 4
                    Text {
            textFormat: Text.PlainText; width: parent.width; text: result.modelData.label; color: "white"; font.bold: true; wrapMode: Text.Wrap }
                    Text {
            textFormat: Text.PlainText;
                        width: parent.width; wrapMode: Text.Wrap; text: result.modelData.detail
                        color: result.modelData.status === "error" ? "#ffb74d" : "#80e5d1"
                    }
                    ActionButton {
                        visible: !!result.modelData.backup; enabled: !dialog.setup.busy
                        text: qsTr("Restaurer le fichier de configuration précédent")
                        onClicked: dialog.setup.restore(result.modelData.backup)
                    }
                }
            }
        }
    }
    Column {
        id: footer
        x: 20; y: parent.height - height - 16; width: parent.width - 40; spacing: 8
        Text {
            textFormat: Text.PlainText; width: parent.width; text: dialog.setup.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#ffb74d" }
        Text {
            textFormat: Text.PlainText; width: parent.width; text: dialog.setup.busy ? qsTr("Opération en cours…") : qsTr("%1 élément(s) sélectionné(s)").arg(dialog.selectionCount); color: "#c9ced6" }
        Row {
            spacing: 12
            ActionButton {
                objectName: "fpsApplySelection"
                text: qsTr("Appliquer la sélection"); selected: dialog.selectionCount > 0
                enabled: !dialog.setup.busy && dialog.selectionCount > 0
                onClicked: { dialog.showResults = true; dialog.setup.apply(Object.keys(dialog.selectedIds)) }
            }
            ActionButton { text: qsTr("Fermer"); enabled: !dialog.setup.busy; onClicked: dialog.close() }
        }
    }
}

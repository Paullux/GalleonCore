// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Galleon Bosun, the settings window. One tab per theme of the pages file, each
// showing the 12 keys as the module draws them and letting the person change them, then
// a tab to add a theme and the tabs for the keyboard lighting, the wallpapers and the
// screensavers. Every change is written to the pages file at once (with a backup), which
// galleond reads by itself: it reaches the module as soon as it is saved, and stays there
// after a restart.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import "TabWidths.js" as TabWidths

Window {
    id: win

    required property var backend
    required property var daemon
    required property var library
    property var languages: null
    property var filePicker: null
    property var updates: null
    property var appearance: null // the appearance of the live lighting (AppearanceStore): written to lighting-appearance.json at every change
    property var keyCalibration: null // the calibration of the key map (KeyCalibration): off until the person starts it
    property bool updatesOpen: false

    property int currentTab: 0
    property int selectedKey: -1
    property bool cropOpen: false // only for a picture of the crop window (--crop)
    // With the icon in the notification area, closing the window only hides it.
    property bool hideOnClose: false
    property bool startMinimized: false
    property bool animateTabs: true
    // The page shown inside a theme's tab: its own, or one of the sub-pages it leads to.
    property string shownPage: ""
    // What the last edit said, in the status bar.
    property string message: ""
    property bool messageIsError: false
    // The last theme looked at: its colour is the keyboard tab's (the left dial of the module shows it).
    property int lastTheme: 0
    // The preview of the lamps of the keyboard (KeyboardMap): lamp id -> colour; only drawn, never sent anywhere.
    property var lampColors: ({})
    // True only while the keys really show the appearance (galleond holds them). Otherwise the drawing is a preview of what they will show.
    readonly property bool keysShowTheDrawing: daemon.lightingState === "holding"
    // Which half of the Keyboard tab is shown: "brightness" (the keyboard's own) or "lighting" (the live lighting, experimental).
    property string keyboardView: "brightness"
    // Where the sunrise of the sky comes from now (AppearanceStore::sunSource): "off", "weather", "sky" or "none". Read again when the drawing is.
    property string sunSource: "off"
    property int selectedCap: -1
    property var selectedLamps: []
    readonly property var lastThemeEntry: backend.themes.length > 0 ? backend.themes[Math.min(lastTheme, backend.themes.length - 1)] : null

    onClosing: close => {
        if (win.hideOnClose) {
            close.accepted = false
            win.visible = false
        }
    }

    title: updates !== null && updates.simulation ? qsTr("Galleon Bosun — Update simulation") : qsTr("Galleon Bosun")
    width: 1240
    height: 740
    minimumWidth: 1000
    minimumHeight: 700
    visible: !startMinimized
    color: "#1b1c1f"

    // One tab per theme, a tab to add one, then the fixed ones.
    readonly property var tabs: backend.themes.map(t => ({title: t.name, kind: "theme", page: t.page, color: t.color})).concat([
        {title: "+", kind: "newtab", page: "", color: "#9aa0a6"},
        {title: qsTr("Keyboard"), kind: "keyboard", page: "", color: "#9aa0a6"},
        {title: qsTr("Wallpapers"), kind: "wallpapers", page: "", color: "#ff00ff"},
        {title: qsTr("Widgets"), kind: "widgets", page: "", color: "#4fc3f7"},
        {title: qsTr("Gaming"), kind: "gaming", page: "", color: "#80e5d1"},
        {title: qsTr("Screensavers"), kind: "screensavers", page: "", color: "#b388ff"}
    ])
    readonly property var tab: tabs[Math.min(currentTab, tabs.length - 1)]
    readonly property string page: tab.kind === "theme" ? (shownPage !== "" && backend.pageNames.indexOf(shownPage) >= 0 ? shownPage : tab.page) : ""
    // The sub-pages that the keys of this tab's page lead to (not the pages of other tabs).
    readonly property var subPages: tab.kind === "theme" ? backend.subPagesOf(tab.page) : []
    readonly property var keys: tab.kind === "theme" ? backend.keys(page) : []
    readonly property var selected: selectedKey >= 0 && selectedKey < keys.length ? keys[selectedKey] : null

    // What an edit answered, for the status bar.
    // The LED lit by a key of the drawing: a cap of the keyboard, or one of the seven sections of the light strip (index 100 + its position).
    function ledOfCap(index) {
        if (index >= 100) {
            for (const section of keyboardMap.map.strip.sections) {
                if (100 + section.position === index)
                    return section.led
            }
            return -1
        }
        const cap = keyboardMap.map.caps[index]
        return cap !== undefined && cap.led !== undefined ? cap.led : -1
    }

    // What the appearance looks like in the drawing: the base on every lamp (a black base is "no colour": dark grey), then the retouches
    // over it, a key with the colour of its LED. The same description galleond plays, drawn once, not moving.
    function previewOfAppearance() {
        if (win.appearance === null)
            return ({})
        let colours = ({})
        if (win.appearance.baseType === "rainbow") {
            colours = keyboardMap.rainbow()
        } else if (win.appearance.baseType === "sky") {
            colours = win.skyPreview()
        } else if (win.appearance.colour.toString() !== "#000000") {
            for (const lamp of keyboardMap.allLamps)
                colours[lamp] = win.appearance.colour.toString()
            for (const section of keyboardMap.map.strip.sections)
                colours[section.lamp] = win.appearance.colour.toString()
        }
        const byLed = {}
        for (const retouch of win.appearance.retouches)
            byLed[retouch.led] = retouch.colour
        for (const cap of keyboardMap.map.caps) {
            if (cap.led !== undefined && byLed[cap.led] !== undefined) {
                for (const lamp of cap.lamps)
                    colours[lamp] = byLed[cap.led]
            }
        }
        for (const section of keyboardMap.map.strip.sections) {
            if (byLed[section.led] !== undefined)
                colours[section.lamp] = byLed[section.led]
        }
        return colours
    }

    // What the live lighting half says about the keyboard's own brightness: it is not touched while the lighting is on (galleond does not write it
    // then), so what is shown is the value read or set before. `value` is the keyboard's unit, 0 to 1000; -1 when it is not known.
    function liveBrightnessText(value) {
        if (value < 0)
            return qsTr("The keyboard's own brightness is not known yet. It stays as it is while the live lighting is on; to change it, switch the live lighting off and use the “brightness” half.")
        return qsTr("The keyboard's own brightness is %1 %. It stays as it is while the live lighting is on; to change it, switch the live lighting off and use the “brightness” half. The level of the colours below is a separate setting.").arg(Math.round(value / 10))
    }

    // The sky at the hour of the computer, as the keyboard shows it: each key takes the colour of its row (the row of the mean height of its
    // lamps), the light strip the dimmed top row. One standing picture: the drawing is redrawn each minute while the sky is the base.
    function skyPreview() {
        const now = new Date()
        const sky = win.appearance.skyAt(now.getHours() + now.getMinutes() / 60 + now.getSeconds() / 3600)
        const heightOf = {}
        for (const cell of keyboardMap.map.cells)
            heightOf[cell.id] = cell.y
        const colours = ({})
        for (const cap of keyboardMap.map.caps) {
            if (cap.lamps.length === 0)
                continue
            let total = 0
            for (const lamp of cap.lamps)
                total += heightOf[lamp]
            const row = win.appearance.skyRowOf(total / cap.lamps.length)
            for (const lamp of cap.lamps)
                colours[lamp] = row < 0 ? sky.strip : sky.rows[row]
        }
        for (const section of keyboardMap.map.strip.sections)
            colours[section.lamp] = sky.strip
        return colours
    }

    function refreshPreview() {
        win.lampColors = win.previewOfAppearance()
        win.sunSource = win.appearance !== null ? win.appearance.sunSource() : "off"
    }

    // Every change is written to the appearance file; with the live lighting on, galleond shows it at once.
    function saved(error) {
        if (error !== "")
            win.answered(error, "")
    }

    function answered(error, done) {
        message = error === "" ? done : error
        messageIsError = error !== ""
    }

    // Another tab: the selection and the sub-page are forgotten. (Not when the list of tabs is
    // read again after an edit: the person is still on the same key.)
    onCurrentTabChanged: {
        selectedKey = -1
        shownPage = ""
        if (currentTab < backend.themes.length)
            lastTheme = currentTab
        // The keyboard tab shows the brightness the keyboard holds now, not the one seen when Bosun started: another program, or
        // the curve of the day, may have changed it. galleond reads it (this window never opens the keyboard) and says so.
        // (Read in the list of tabs: `tab` may not have followed the new index yet when this handler runs.)
        if (tabs[currentTab] !== undefined && tabs[currentTab].kind === "keyboard")
            win.daemon.readKeyboardBrightness()
    }

    FontMetrics { id: tabFont; font.pixelSize: 14 }

    // ---- the tab bar
    Flickable {
        id: tabViewport
        objectName: "tabViewport"
        readonly property var tabWidths: TabWidths.distribute(win.tabs.map(tab => Math.ceil(tabFont.advanceWidth(tab.title)) + 24),
                                                             win.currentTab, width, bar.spacing, 70)
        function revealSelected() {
            const selected = tabRepeater.itemAt(win.currentTab)
            if (!selected) return
            const left = selected.x
            const right = left + selected.width
            const target = left < contentX ? left : right > contentX + width ? right - width : contentX
            contentX = Math.max(0, Math.min(target, Math.max(0, contentWidth - width)))
        }
        Timer {
            id: revealTimer
            interval: 0
            repeat: false
            onTriggered: tabViewport.revealSelected()
        }
        onTabWidthsChanged: revealTimer.start()
        onContentWidthChanged: revealTimer.start()
        x: 20; y: 16
        width: win.width - 224; height: 44
        contentWidth: bar.width; contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Row {
        id: bar
        objectName: "tabBar"
        height: tabViewport.height
        spacing: 4

        Repeater {
            id: tabRepeater
            model: win.tabs
            delegate: Rectangle {
                id: tabButton
                required property var modelData
                required property int index
                readonly property bool current: tabButton.index === win.currentTab
                readonly property bool resizing: resizeAnimation.running
                readonly property color accent: modelData.kind === "keyboard" && win.lastTheme < win.backend.themes.length
                                                ? win.backend.themes[win.lastTheme].color : modelData.color
                onCurrentChanged: revealTimer.start()
                onXChanged: if (current) revealTimer.start()
                onWidthChanged: if (current) revealTimer.start()
                Behavior on width {
                    enabled: win.animateTabs
                    NumberAnimation { id: resizeAnimation; duration: 1000; easing.type: Easing.InOutCubic }
                }
                width: tabViewport.tabWidths[tabButton.index] || 70
                height: bar.height
                radius: 8
                color: tabButton.current ? "#2a2d32" : (hover.containsMouse ? "#232529" : "transparent")

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 16
                    height: 3
                    radius: 1.5
                    color: tabButton.accent
                    opacity: tabButton.current ? 1 : 0.45
                }
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    color: tabButton.current ? "white" : "#b8bec6"
                    font.pixelSize: 14
                    text: tabButton.modelData.title
                    Accessible.name: text
                }
                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: win.currentTab = tabButton.index
                }
                Accessible.role: Accessible.PageTab
                Accessible.name: tabButton.modelData.title
            }
        }
        }
    }

    Rectangle {
        x: 20
        y: 64
        width: win.width - 40
        height: 1
        color: "#33ffffff"
    }

    // ---- a problem with the pages file hides everything else
    Text {
        visible: win.backend.problem !== ""
        x: 30
        y: 90
        width: win.width - 60
        color: "#ff8a80"
        font.pixelSize: 14
        wrapMode: Text.Wrap
        text: win.backend.problem
    }

    // ---- a theme: the 12 keys, and on the right what the selected key does or the tab itself
    Item {
        id: themePage
        objectName: "themePage"
        visible: win.tab.kind === "theme" && win.backend.problem === ""
        // The size of a key: 140 px, smaller in a low window so that the four rows end above the status bar and none of the keys
        // is hidden under it (never below 84 px: a key has to stay readable and hit).
        readonly property real cellSize: Math.max(84, Math.min(140, (height - keyGrid.y - 3 * keyGrid.spacing) / 4))
        // The left column keeps the width of three full-size keys whatever their size: the note above them wraps at that width
        // (its height decides where the keys start, so it cannot depend on the keys' size: that would be a loop), and the
        // right column does not move when the window is lowered.
        readonly property real gridWidth: 3 * 140 + 2 * 12
        x: 30
        y: 84
        width: win.width - 60
        height: win.height - 104 - statusBar.height

        // The pages of the tab: its own and the sub-pages that keys lead to.
        Row {
            id: pageChoice
            spacing: 6
            Text {
                height: 30
                verticalAlignment: Text.AlignVCenter
                color: "#9aa0a6"
                font.pixelSize: 13
                text: qsTr("Page")
            }
            Pill {
                text: win.tab.page
                selected: win.page === win.tab.page
                onClicked: win.shownPage = win.tab.page
            }
            Repeater {
                model: win.subPages
                delegate: Pill {
                    required property string modelData
                    text: modelData
                    selected: win.page === modelData
                    onClicked: {
                        win.shownPage = modelData
                        win.selectedKey = -1
                    }
                }
            }
        }
        Text {
            id: pageNote
            y: pageChoice.height + 4
            width: themePage.gridWidth
            color: "#9aa0a6"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            text: {
                const fill = win.backend.autoFill(win.page)
                return fill === "" ? "" : qsTr("The module also adds: %1.").arg(fill)
            }
        }

        Grid {
            id: keyGrid
            y: pageChoice.height + pageNote.height + 12
            columns: 3
            spacing: 12

            Repeater {
                model: win.keys
                delegate: Rectangle {
                    id: keyCell
                    objectName: "keyCell"
                    required property var modelData
                    width: themePage.cellSize
                    height: themePage.cellSize
                    radius: 14
                    color: "transparent"
                    border.width: win.selectedKey === keyCell.modelData.index ? 3 : 0
                    border.color: win.tab.color

                    Image {
                        anchors.fill: parent
                        anchors.margins: win.selectedKey === keyCell.modelData.index ? 3 : 0
                        asynchronous: false
                        cache: false
                        smooth: true
                        source: "image://tiles/" + win.page + "/" + keyCell.modelData.index + "?r=" + win.backend.revision + "&theme=" + win.tab.page
                    }
                    // The application of this key is not installed on this computer: the key is kept and does nothing.
                    Rectangle {
                        objectName: "missingApp"
                        visible: keyCell.modelData.appMissing === true
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 6
                        width: 26
                        height: 26
                        radius: 13
                        color: "#e65100"
                        border.width: 2
                        border.color: "#1b1d20"
                        Text {
                            anchors.centerIn: parent
                            text: "!"
                            color: "white"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: win.selectedKey = keyCell.modelData.index
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: keyCell.modelData.empty ? qsTr("Key %1, empty").arg(keyCell.modelData.index + 1)
                                     : keyCell.modelData.appMissing === true
                                       ? qsTr("Key %1, %2, application not installed").arg(keyCell.modelData.index + 1).arg(keyCell.modelData.label)
                                       : qsTr("Key %1, %2").arg(keyCell.modelData.index + 1).arg(keyCell.modelData.label)
                }
            }
        }

        // The right side: the key that is selected, else the tab itself.
        Flickable {
            id: rightSide
            x: themePage.gridWidth + 40
            width: parent.width - x
            height: parent.height
            contentWidth: width
            contentHeight: rightColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: rightColumn
                width: rightSide.width - 12
                spacing: 22

                KeyEditor {
                    objectName: "keyEditor"
                    filePicker: win.filePicker
                    visible: win.selected !== null
                    width: parent.width
                    backend: win.backend
                    page: win.page
                    key: win.selected !== null ? win.selected : ({index: 0, empty: true, type: "none"})
                    onAnswered: (error, done) => win.answered(error, done)
                }
                Text {
                    visible: win.selected === null
                    color: "#9aa0a6"
                    font.pixelSize: 14
                    text: qsTr("Choose a key to change what it does and shows.")
                }
                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#33ffffff"
                }
                TabEditor {
                    objectName: "tabEditor"
                    width: parent.width
                    filePicker: win.filePicker
                    backend: win.backend
                    library: win.library
                    index: Math.min(win.currentTab, win.backend.themes.length - 1)
                    theme: win.backend.themes[Math.min(win.currentTab, win.backend.themes.length - 1)]
                    onAnswered: (error, done) => win.answered(error, done)
                    onMoved: to => win.currentTab = to
                    onRemoved: win.currentTab = Math.max(0, win.currentTab - 1)
                }
            }
        }
    }

    // ---- a new tab
    NewTab {
        visible: win.tab.kind === "newtab" && win.backend.problem === ""
        x: 30
        y: 90
        width: Math.min(win.width - 60, 560)
        backend: win.backend
        filePicker: win.filePicker
        onAnswered: (error, done) => win.answered(error, done)
        onCreated: win.currentTab = win.backend.themes.length - 1
    }

    ScrollRail {
        objectName: "themeScrollRail"
        flickable: rightSide
        active: themePage.visible
        x: themePage.x + themePage.width - width
        y: themePage.y
        height: themePage.height
    }

    // A theme was installed: show its tab here, and on the module.
    Connections {
        target: win.backend
        function onThemeInstalled(themeIndex, page) {
            win.currentTab = themeIndex
            win.daemon.showTheme(page)
        }
    }

    // ---- the keyboard: taller than the window once the curve editor is there, so it scrolls (wheel, bar, drag)
    Item {
        id: keyboardPage
        objectName: "keyboardPage"
        visible: win.tab.kind === "keyboard" && win.backend.problem === ""
        x: 30
        y: 84
        width: win.width - 60
        height: win.height - 104 - statusBar.height

        Flickable {
            id: keyboardScroll
            objectName: "keyboardScroll"
            anchors.fill: parent
            anchors.rightMargin: 18  // room for the bars, always: a bar that comes and goes would move the content
            anchors.bottomMargin: 18
            // What does not fit in width is reached by the bar at the bottom: a keyboard drawn for per-key settings
            // or theme demos will need room that a narrow window does not have.
            contentWidth: Math.max(width, keyboardColumn.childrenRect.width)
            contentHeight: keyboardColumn.height + 24
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: keyboardColumn
                y: 6
                width: Math.min(keyboardScroll.width, 760)
                spacing: 12

                Text {
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                    text: qsTr("Keyboard and Stream Deck module")
                }
                Text {
                    objectName: "firmwareVersion"
                    width: parent.width
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: win.backend.homeWidgets.firmwareWarning ? "#ffb74d" : "#c9ced6"
                    text: win.backend.homeWidgets.firmwareVersion
                          ? (win.backend.homeWidgets.firmwareSimulation ? qsTr("Simulation — ") : "") + qsTr("Stream Deck firmware: %1").arg(win.backend.homeWidgets.firmwareVersion)
                          : qsTr("Stream Deck firmware: unknown — connect the module and start the updated daemon.")
                }
                Text {
                    objectName: "firmwareWarning"
                    visible: win.backend.homeWidgets.firmwareWarning
                    width: parent.width
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: "#ffb74d"
                    text: qsTr("An older Stream Deck firmware was detected. Version 3.06.005 has caused a Windows freeze on the owner's device. Update to 3.06.006 or later using the official Elgato Stream Deck app. Fully stop Bosun and galleond first; keep the keyboard connected during the update. Then close Elgato and disable its automatic startup (or uninstall it if unused) before restarting GalleonCore. This is separate from the keyboard firmware managed by Corsair Web Hub.")
                }
                Pill {
                    objectName: "downloadElgatoStreamDeck"
                    visible: win.backend.homeWidgets.windowsCapture || win.backend.homeWidgets.firmwareSimulation
                    selected: win.backend.homeWidgets.firmwareWarning
                    text: qsTr("Download Elgato Stream Deck 7.6.0 for Windows (.msi)")
                    onClicked: Qt.openUrlExternally("https://edge.elgato.com/egc/windows/sd/Stream_Deck_7.6.0.23012.msi")
                }
                Text {
                    visible: win.backend.homeWidgets.windowsCapture || win.backend.homeWidgets.firmwareSimulation
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: qsTr("7.6.0 is the Elgato application version, not the Stream Deck firmware version. Firmware updates are performed inside the Elgato app after fully stopping Bosun and galleond.")
                }
                Rectangle { width: parent.width; height: 1; color: "#33ffffff" }
                Row {
                    spacing: 8
                    Pill {
                        objectName: "viewBrightness"
                        selected: win.keyboardView === "brightness"
                        text: qsTr("Corsair keyboard · brightness")
                        onClicked: win.keyboardView = "brightness"
                    }
                    Pill {
                        objectName: "viewLighting"
                        selected: win.keyboardView === "lighting"
                        text: qsTr("Corsair keyboard · live lighting (experimental)")
                        onClicked: win.keyboardView = "lighting"
                    }
                }
                Column {
                    objectName: "brightnessView"
                    width: parent.width
                    spacing: 12
                    visible: win.keyboardView === "brightness"
                Text {
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Corsair keyboard · brightness")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: qsTr("The brightness of the keyboard's own lighting, in steps of 10 %. galleond writes it when you let go of the slider.")
                }
                BrightnessSlider {
                    objectName: "keyboardBrightness"
                    id: keyboardBrightnessSlider
                    width: Math.min(parent.width, 560)
                    // In curve mode the curve is in charge, not this slider.
                    enabledLook: win.daemon.running && win.backend.brightnessCurve.mode === "slider" && !win.daemon.lightingActive
                    // What galleond read from the keyboard (or what was just set), unless a stop is being picked now.
                    Connections {
                        target: win.daemon
                        function onKeyboardBrightnessChanged() {
                            if (win.daemon.keyboardBrightness >= 0 && keyboardBrightnessSlider.preview < 0)
                                keyboardBrightnessSlider.value = win.daemon.keyboardBrightness
                        }
                    }
                    Component.onCompleted: if (win.daemon.keyboardBrightness >= 0) value = win.daemon.keyboardBrightness
                    onCommitted: value => win.answered(win.daemon.setKeyboardBrightness(value),
                                                       qsTr("Keyboard brightness sent to galleond: %1 %").arg(value / 10))
                }
                Text {
                    objectName: "brightnessSuspendedNote"
                    visible: win.daemon.lightingActive
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffb74d"
                    font.pixelSize: 13
                    text: qsTr("The live lighting is on and holds the keyboard: the brightness is suspended, and the slider and the curve do nothing until it is switched off.")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: win.daemon.running
                          ? qsTr("The keyboard keeps this value when it is unplugged, as it does for the Web Hub. The slider shows the value galleond read from the keyboard when it started, and what you set since. Close the Web Hub first, or its own answers may hide this one.")
                          : qsTr("Start galleond (above) to change the brightness: this window never talks to the keyboard itself.")
                }
                Row {
                    spacing: 28
                    RadioOption {
                        objectName: "brightnessModeSlider"
                        text: qsTr("Manual slider")
                        checked: win.backend.brightnessCurve.mode === "slider"
                        onChosen: win.answered(win.backend.brightnessCurve.setMode("slider"),
                                               qsTr("The slider is in charge of the keyboard brightness."))
                    }
                    RadioOption {
                        objectName: "brightnessModeCurve"
                        text: qsTr("Curve over 24 hours")
                        checked: win.backend.brightnessCurve.mode === "curve"
                        onChosen: win.answered(win.backend.brightnessCurve.setMode("curve"),
                                               qsTr("The curve is in charge of the keyboard brightness."))
                    }
                }
                Text {
                    objectName: "brightnessCurveProblem"
                    visible: win.backend.brightnessCurve.problem !== ""
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffb74d"
                    font.pixelSize: 13
                    text: win.backend.brightnessCurve.problem
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: qsTr("In curve mode galleond sets the keyboard to the point nearest to the time of day, at most once an hour, and the slider above is not used. Drag the points up and down: the keyboard receives the nearest step of 10 % for each hour. The curve is saved as you go, whichever mode is chosen.")
                }
                BrightnessCurveEditor {
                    objectName: "brightnessCurveEditor"
                    width: Math.min(parent.width, 720)
                    curveModel: win.backend.brightnessCurve
                    onCommitted: (hour, value) => win.answered(win.backend.brightnessCurve.setHour(hour, value),
                                                               qsTr("Brightness at %1 h: %2 %").arg(hour).arg(value / 10))
                }
                }
                Column {
                    objectName: "lightingView"
                    width: parent.width
                    spacing: 12
                    visible: win.keyboardView === "lighting"
                Text {
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Corsair keyboard · live lighting (experimental)")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: win.daemon.lightingSupported
                          ? qsTr("galleond can light the keys itself, one colour for all of them. It is off by default and never writes to the keyboard's memory: when it stops, or is stopped, the keys go back to what the keyboard held. A small watchdog program gives the keys back even if galleond crashes. Switch it on only if you accept that this is experimental.")
                          : qsTr("The live lighting is not available on this system.")
                }
                Pill {
                    objectName: "lightingToggle"
                    visible: win.daemon.lightingSupported
                    enabled: win.daemon.running
                    selected: win.daemon.lightingActive
                    text: win.daemon.lightingActive ? qsTr("Switch the live lighting off") : qsTr("Switch the live lighting on")
                    onClicked: win.answered(win.daemon.enableLighting(!win.daemon.lightingActive),
                                            win.daemon.lightingActive ? qsTr("Asked galleond to give the keys back.")
                                                                      : qsTr("Asked galleond to light the keys."))
                }
                Text {
                    objectName: "lightingState"
                    visible: win.daemon.lightingSupported
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: win.daemon.lightingDetail !== "" && !win.daemon.lightingActive ? "#ffb74d" : "#9aa0a6"
                    font.pixelSize: 13
                    text: {
                        const names = {
                            "off": qsTr("Live lighting: off"),
                            "starting": qsTr("Live lighting: starting"),
                            "listening": qsTr("Live lighting: waiting for the keyboard to be free"),
                            "taking": qsTr("Live lighting: taking the keys"),
                            "holding": qsTr("Live lighting: on"),
                            "yielded": qsTr("Live lighting: given back to another program"),
                            "sleeping": qsTr("Live lighting: paused while the computer sleeps")
                        }
                        const base = names[win.daemon.lightingState] !== undefined ? names[win.daemon.lightingState] : win.daemon.lightingState
                        return win.daemon.lightingDetail !== "" ? base + " · " + win.daemon.lightingDetail : base
                    }
                }
                Text {
                    objectName: "liveBrightnessNote"
                    visible: win.daemon.lightingSupported
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: win.liveBrightnessText(win.daemon.keyboardBrightness)
                }
                Swatches {
                    objectName: "lightingSwatches"
                    visible: win.daemon.lightingSupported && win.daemon.lightingActive
                    color: "#ffffff"
                    onPicked: picked => {
                        const c = Qt.color(picked)
                        win.saved(win.appearance.clearRetouches())
                        win.answered(win.appearance.setUniform(Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)),
                                     win.daemon.lightingActive ? qsTr("Keyboard colour sent to galleond: %1").arg(picked)
                                                               : qsTr("Keyboard colour kept for the next time the live lighting is on: %1").arg(picked))
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#33ffffff" }
                Text {
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Keyboard and module · preview")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: qsTr("As on the keyboard, the legend and the outline of each key are see-through: the light of the lamps under it shows through. Click a key and pick a colour to see how a lighting would look. The module shows the keys of the theme tab you looked at last.")
                }
                Text {
                    color: "white"
                    font.pixelSize: 14
                    font.bold: true
                    text: qsTr("Appearance")
                }
                Row {
                    spacing: 8
                    Pill {
                        objectName: "appearanceUniform"
                        selected: win.appearance !== null && win.appearance.baseType === "uniform"
                        text: qsTr("One colour")
                        onClicked: {
                            const c = win.appearance.colour
                            win.saved(win.appearance.setUniform(Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)))
                        }
                    }
                    Pill {
                        objectName: "appearanceSky"
                        selected: win.appearance !== null && win.appearance.baseType === "sky"
                        text: qsTr("Sky by the hour")
                        onClicked: win.saved(win.appearance.setSky())
                    }
                    Pill {
                        objectName: "appearanceRainbow"
                        selected: win.appearance !== null && win.appearance.baseType === "rainbow"
                        text: qsTr("Rainbow")
                        onClicked: win.saved(win.appearance.setRainbow(win.appearance.rainbowSpeed))
                    }
                }
                Row {
                    objectName: "rainbowSpeeds"
                    spacing: 8
                    visible: win.appearance !== null && win.appearance.baseType === "rainbow"
                    Repeater {
                        // Thousandths of a turn per second: fixed, one turn in 50 s, in 10 s, in 3 s.
                        model: [{speed: 0, name: qsTr("Fixed")}, {speed: 20, name: qsTr("Slow")}, {speed: 100, name: qsTr("Medium")}, {speed: 300, name: qsTr("Fast")}]
                        Pill {
                            required property var modelData
                            objectName: "rainbowSpeed" + modelData.speed
                            selected: win.appearance !== null && win.appearance.rainbowSpeed === modelData.speed
                            text: modelData.name
                            onClicked: win.saved(win.appearance.setRainbow(modelData.speed))
                        }
                    }
                }
                Text {
                    objectName: "skyNote"
                    width: parent.width
                    wrapMode: Text.Wrap
                    visible: win.appearance !== null && win.appearance.baseType === "sky"
                    color: "#7d8591"
                    font.pixelSize: 13
                    text: qsTr("The sky follows the hour of this computer: night, dawn, day, dusk, one colour for each row of keys and the light strip dimmed. By day the keys shimmer a little; by night a few stars drift along the rows and some keys twinkle. The palette, the shimmer and the stars come from the mockup of Aaryush1 (issue #111). The drawing shows the sky standing still, and the colours are not tuned on the real LEDs.")
                }
                Pill {
                    objectName: "skyAnimated"
                    visible: win.appearance !== null && win.appearance.baseType === "sky"
                    selected: win.appearance !== null && win.appearance.skyAnimated
                    text: qsTr("Shimmer and stars")
                    onClicked: win.saved(win.appearance.setSkyAnimated(!win.appearance.skyAnimated))
                }
                Pill {
                    objectName: "sunFollow"
                    visible: win.appearance !== null && win.appearance.baseType === "sky"
                    selected: win.appearance !== null && win.appearance.followSun
                    text: qsTr("Follow the sunrise and the sunset")
                    onClicked: win.saved(win.appearance.setFollowSun(!win.appearance.followSun))
                }
                Column {
                    objectName: "sunPlace"
                    width: parent.width
                    spacing: 8
                    visible: win.appearance !== null && win.appearance.baseType === "sky" && win.appearance.followSun
                    Text {
                        objectName: "sunSourceNote"
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: win.sunSource === "none" ? "#ffb74d" : "#c9ced6"
                        font.pixelSize: 13
                        text: win.sunSource === "weather" ? qsTr("The place of the weather widget is used to find the sunrise and the sunset.")
                              : win.sunSource === "sky" ? qsTr("The place typed below is used to find the sunrise and the sunset.")
                              : qsTr("No place is known: the fixed hours (dawn at 6 h 15, dusk at 18 h) are used. Type a place below, or set one for the weather widget.")
                    }
                    Row {
                        spacing: 8
                        Field {
                            id: skyLatitude
                            objectName: "skyLatitude"
                            value: win.appearance !== null ? win.appearance.skyLatitude : ""
                            placeholder: qsTr("Latitude (e.g. 48.85)")
                        }
                        Field {
                            id: skyLongitude
                            objectName: "skyLongitude"
                            value: win.appearance !== null ? win.appearance.skyLongitude : ""
                            placeholder: qsTr("Longitude (e.g. 2.35)")
                        }
                    }
                    Row {
                        spacing: 8
                        Pill {
                            objectName: "skyPlaceSet"
                            text: qsTr("Use this place")
                            onClicked: win.answered(win.appearance.setSkyPlace(skyLatitude.input.text, skyLongitude.input.text), qsTr("The place of the sky is saved on this computer."))
                        }
                        Pill {
                            objectName: "skyPlaceForget"
                            enabledLook: win.appearance !== null && win.appearance.hasSkyPlace
                            text: qsTr("Forget this place")
                            onClicked: win.saved(win.appearance.clearSkyPlace())
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: "#7d8591"
                        font.pixelSize: 13
                        text: qsTr("A place is personal data. The one typed here stays in the appearance file on this computer, is only used to compute the sunrise and the sunset, and is never sent anywhere. The place of the weather widget is read only while this is on, and it comes first.")
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    visible: win.appearance !== null && win.appearance.baseType === "rainbow" && win.appearance.rainbowSpeed > 0
                    color: "#7d8591"
                    font.pixelSize: 13
                    text: qsTr("The rainbow moves on the keyboard; the drawing shows it standing still.")
                }
                KeyboardMap {
                    id: keyboardMap
                    objectName: "keyboardMap"
                    width: Math.min(parent.width, 740)
                    opacity: win.keysShowTheDrawing ? 1 : 0.4 // greyed: what the keyboard shows now is not this
                    colors: win.keysShowTheDrawing ? win.lampColors : ({}) // black and grey while the keys do not show it
                    enabled: win.keysShowTheDrawing // and not to be edited blind
                    selected: win.selectedCap
                    themeName: win.lastThemeEntry ? win.lastThemeEntry.name : ""
                    themeColor: win.lastThemeEntry ? win.lastThemeEntry.color : "#9aa0a6"
                    tileSource: index => win.lastThemeEntry
                        ? "image://tiles/" + win.lastThemeEntry.page + "/" + index + "?r=" + win.backend.revision + "&theme=" + win.lastThemeEntry.page
                        : ""
                    onCapClicked: (index, lamps) => { win.selectedCap = index; win.selectedLamps = lamps }
                    Component.onCompleted: win.refreshPreview()
                    Connections {
                        target: win.appearance
                        function onChanged() { win.refreshPreview() }
                    }
                    Timer { // the sky changes with the hour: the drawing follows each minute
                        interval: 60000
                        repeat: true
                        running: win.appearance !== null && win.appearance.baseType === "sky"
                        onTriggered: win.refreshPreview()
                    }
                }
                Text {
                    objectName: "keySelection"
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: win.selectedCap < 0 ? qsTr("No key chosen: click a key.")
                                              : qsTr("Key chosen (lamps: %1).").arg(win.selectedLamps.join(", "))
                }
                Swatches {
                    objectName: "lampSwatches"
                    visible: win.selectedCap >= 0
                    onPicked: color => {
                        const led = win.ledOfCap(win.selectedCap)
                        if (led < 0 || win.appearance === null)
                            return
                        const c = Qt.color(color)
                        win.saved(win.appearance.retouch(led, Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)))
                    }
                }
                Row {
                    spacing: 8
                    Pill {
                        objectName: "lampsColourAll"
                        enabledLook: win.selectedCap >= 0 && win.selectedLamps.length > 0 && win.lampColors[win.selectedLamps[0]] !== undefined
                        text: qsTr("Give this colour to every key")
                        onClicked: {
                            const c = Qt.color(win.lampColors[win.selectedLamps[0]])
                            win.saved(win.appearance.clearRetouches())
                            win.saved(win.appearance.setUniform(Math.round(c.r * 255), Math.round(c.g * 255), Math.round(c.b * 255)))
                        }
                    }
                    Pill {
                        objectName: "lampsClear"
                        text: qsTr("Clear the colours")
                        onClicked: { win.selectedCap = -1; win.selectedLamps = []; win.saved(win.appearance.reset()) }
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#7d8591"
                    font.pixelSize: 13
                    text: win.keysShowTheDrawing
                          ? qsTr("The live lighting is on: the colours you give to the keys here go to the keyboard, one colour for each key; a key with no colour stays dark.")
                          : win.daemon.lightingActive
                          ? qsTr("The drawing is greyed: the keyboard is not showing it right now (see the state of the live lighting above). What you choose here is kept and shown as soon as it is.")
                          : qsTr("The drawing stays in black and grey while the live lighting is off: switch it on to see and choose the colours of the keys. The key caps are a standard French layout drawn for orientation; the lamps under them are at the positions the keyboard reports.")
                }
                Text {
                    color: "#c9ced6"
                    font.pixelSize: 13
                    text: qsTr("Level of the colours: how strongly the colours above are shown, without changing the brightness of the keyboard itself.")
                    width: parent.width
                    wrapMode: Text.Wrap
                }
                BrightnessSlider {
                    id: levelSlider
                    objectName: "appearanceLevel"
                    width: Math.min(parent.width, 560)
                    accessibleName: qsTr("Level of the colours")
                    value: win.appearance !== null ? win.appearance.level : 1000
                    // The slider sets its own value when it commits, which cuts the binding: follow the appearance again after any change.
                    Connections {
                        target: win.appearance
                        function onChanged() { if (win.appearance !== null) levelSlider.value = win.appearance.level }
                    }
                    onCommitted: level => win.saved(win.appearance.setLevel(level))
                }
                Row {
                    spacing: 28
                    RadioOption {
                        objectName: "levelModeSlider"
                        text: qsTr("Manual slider")
                        checked: win.backend.levelCurve.mode === "slider"
                        onChosen: win.answered(win.backend.levelCurve.setMode("slider"), qsTr("The slider above sets the level of the colours."))
                    }
                    RadioOption {
                        objectName: "levelModeCurve"
                        text: qsTr("Curve over 24 hours")
                        checked: win.backend.levelCurve.mode === "curve"
                        onChosen: win.answered(win.backend.levelCurve.setMode("curve"), qsTr("The curve sets the level of the colours."))
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: qsTr("In curve mode galleond shows the colours at the level of the point nearest to the time of day, instead of the slider above. It only dims the colours: nothing is written to the memory of the keyboard. This curve is separate from the one of the keyboard brightness.")
                }
                BrightnessCurveEditor {
                    objectName: "levelCurveEditor"
                    width: Math.min(parent.width, 720)
                    curveModel: win.backend.levelCurve
                    curveName: qsTr("Level of the colours through the day")
                    pointName: qsTr("Level at %1 h")
                    onCommitted: (hour, value) => win.answered(win.backend.levelCurve.setHour(hour, value),
                                                               qsTr("Level at %1 h: %2 %").arg(hour).arg(value / 10))
                }
                Rectangle { width: parent.width; height: 1; color: "#33ffffff"; visible: win.keyCalibration !== null }
                Text {
                    visible: win.keyCalibration !== null
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Calibrate the keys")
                }
                Text {
                    visible: win.keyCalibration !== null
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: qsTr("galleond lights one LED at a time and you press the key that lit up. The answers build the map between the LEDs and your keys. Only this window reads the keys, only while the calibration is open, and nothing you press is kept: the file holds which LED is which key, nothing else.")
                }
                Row {
                    spacing: 8
                    visible: win.keyCalibration !== null && !win.keyCalibration.active
                    Pill {
                        objectName: "calibrationBegin"
                        enabledLook: win.daemon.lightingActive
                        text: qsTr("Calibrate the keys")
                        onClicked: {
                            win.answered(win.keyCalibration.begin(false), qsTr("Calibration started."))
                            calibrationPanel.forceActiveFocus()
                        }
                    }
                    Pill {
                        objectName: "calibrationRestart"
                        enabledLook: win.daemon.lightingActive
                        text: qsTr("Start over")
                        onClicked: {
                            win.answered(win.keyCalibration.begin(true), qsTr("Calibration started over."))
                            calibrationPanel.forceActiveFocus()
                        }
                    }
                }
                Text {
                    objectName: "calibrationNeedsLighting"
                    visible: win.keyCalibration !== null && !win.keyCalibration.active && !win.daemon.lightingActive
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffb74d"
                    font.pixelSize: 13
                    text: qsTr("Switch the live lighting on first: the calibration lights one LED at a time.")
                }
                Item {
                    id: calibrationPanel
                    objectName: "calibrationPanel"
                    property int stripPosition: 1
                    visible: win.keyCalibration !== null && win.keyCalibration.active
                    width: parent.width
                    height: visible ? calibrationColumn.height : 0
                    focus: visible
                    onVisibleChanged: if (visible) forceActiveFocus()
                    // The key of this window that has the focus: the code of the physical key, never the character. Nothing is kept.
                    Keys.onPressed: event => {
                        if (win.keyCalibration !== null && !event.isAutoRepeat)
                            win.keyCalibration.handleKey(event.nativeScanCode)
                        event.accepted = true
                    }
                    Column {
                        id: calibrationColumn
                        width: parent.width
                        spacing: 8
                        Text {
                            objectName: "calibrationProgress"
                            color: "white"
                            font.pixelSize: 15
                            text: win.keyCalibration === null ? "" : win.keyCalibration.finished
                                  ? qsTr("Done: all %1 LEDs have been passed.").arg(win.keyCalibration.total)
                                  : qsTr("LED %1 is lit. %2 of %3 passed, %4 skipped.").arg(win.keyCalibration.currentLed).arg(win.keyCalibration.passed).arg(win.keyCalibration.total).arg(win.keyCalibration.skipped)
                        }
                        Text {
                            objectName: "calibrationMessage"
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: "#c9ced6"
                            font.pixelSize: 13
                            text: win.keyCalibration === null ? "" : win.keyCalibration.message
                        }
                        Row {
                            spacing: 8
                            Pill {
                                objectName: "calibrationNone"
                                text: qsTr("Nothing lit up")
                                onClicked: { win.keyCalibration.answerNone(); calibrationPanel.forceActiveFocus() }
                            }
                            Pill {
                                objectName: "calibrationSkip"
                                text: qsTr("Skip")
                                onClicked: { win.keyCalibration.skip(); calibrationPanel.forceActiveFocus() }
                            }
                            Pill {
                                objectName: "calibrationBack"
                                text: qsTr("Back")
                                onClicked: { win.keyCalibration.back(); calibrationPanel.forceActiveFocus() }
                            }
                            Pill {
                                objectName: "calibrationStop"
                                danger: true
                                text: qsTr("Stop")
                                onClicked: win.keyCalibration.end()
                            }
                        }
                        Row {
                            spacing: 8
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#c9ced6"
                                font.pixelSize: 13
                                text: qsTr("A section of the light strip lit up, counted from the left:")
                            }
                            Pill {
                                objectName: "calibrationStripLess"
                                text: "−"
                                onClicked: { calibrationPanel.stripPosition = Math.max(1, calibrationPanel.stripPosition - 1); calibrationPanel.forceActiveFocus() }
                            }
                            Text {
                                objectName: "calibrationStripPosition"
                                anchors.verticalCenter: parent.verticalCenter
                                color: "white"
                                font.pixelSize: 15
                                text: calibrationPanel.stripPosition
                            }
                            Pill {
                                objectName: "calibrationStripMore"
                                text: "+"
                                onClicked: { calibrationPanel.stripPosition = Math.min(64, calibrationPanel.stripPosition + 1); calibrationPanel.forceActiveFocus() }
                            }
                            Pill {
                                objectName: "calibrationStrip"
                                text: qsTr("It is that section")
                                onClicked: { win.keyCalibration.answerStrip(calibrationPanel.stripPosition); calibrationPanel.forceActiveFocus() }
                            }
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: "#c9ced6"
                            font.pixelSize: 13
                            text: qsTr("A key that the system keeps from this window, or that sends no code of its own: choose it here.")
                        }
                        Flow {
                            width: parent.width
                            spacing: 8
                            Repeater {
                                model: win.keyCalibration !== null ? win.keyCalibration.specialKeys() : []
                                delegate: Pill {
                                    required property var modelData
                                    objectName: "calibrationSpecial" + modelData.usage
                                    text: modelData.label
                                    onClicked: { win.keyCalibration.answerNamedKey(modelData.usage); calibrationPanel.forceActiveFocus() }
                                }
                            }
                        }
                    }
                }
                }
                Rectangle { width: parent.width; height: 1; color: "#33ffffff" }
                Text {
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Corsair keyboard · lighting and keys · manufacturer's solution")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#c9ced6"
                    font.pixelSize: 14
                    text: qsTr("Corsair's own page, the Web Hub, sets the keyboard's lighting and keys in your browser. It needs a Chromium-based browser (Chrome, Chromium, Edge, Opera).")
                }
                Pill {
                    objectName: "openWebHub"
                    selected: true
                    text: qsTr("Open the Corsair Web Hub in your browser")
                    onClicked: Qt.openUrlExternally("https://www.corsair.com/web-hub")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#9aa0a6"
                    font.pixelSize: 13
                    text: qsTr("It opens in your browser, not in this window. Careful: what the Web Hub saves into the keyboard's own memory is the keyboard's business, and this application never writes there. The screen, the 12 keys and the dials of the module are set from the other tabs.")
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#7d8591"
                    font.pixelSize: 13
                    text: qsTr("The lighting can also be driven from Linux with OpenRGB (the dials' rings already are, through this project's plugin); the per-key lighting of the keyboard is not available there yet.")
                }
    
            }
        }
        ScrollRail {
            objectName: "keyboardScrollRail"
            flickable: keyboardScroll
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
        }
        ScrollRail {
            objectName: "keyboardScrollRailHorizontal"
            orientation: Qt.Horizontal
            flickable: keyboardScroll
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 18
        }
    }

    WallpapersPage {
        id: wallpapersPage
        filePicker: win.filePicker
        visible: win.tab.kind === "wallpapers" && win.backend.problem === ""
        x: 30
        y: 84
        width: win.width - 78
        height: win.height - 104 - statusBar.height
        library: win.library
        backend: win.backend
    }
    ScrollRail {
        objectName: "wallpapersPageScrollRail"
        flickable: wallpapersPage
        active: wallpapersPage.visible
        x: wallpapersPage.x + wallpapersPage.width + 8
        y: wallpapersPage.y
        height: wallpapersPage.height
    }

    WidgetsPage {
        id: widgetsPage
        visible: win.tab.kind === "widgets"
        x: 30; y: 84
        width: win.width - 78
        height: win.height - 104 - statusBar.height
        widgets: win.backend.homeWidgets
    }
    ScrollRail {
        objectName: "widgetsPageScrollRail"
        flickable: widgetsPage
        active: widgetsPage.visible
        x: widgetsPage.x + widgetsPage.width + 8
        y: widgetsPage.y
        height: widgetsPage.height
    }

    GamingPage {
        id: gamingPage
        visible: win.tab.kind === "gaming"
        x: 30; y: 84
        width: win.width - 78
        height: win.height - 104 - statusBar.height
        recording: win.backend.recording
        widgets: win.backend.homeWidgets
        daemon: win.daemon
    }
    ScrollRail {
        objectName: "gamingPageScrollRail"
        flickable: gamingPage
        active: gamingPage.visible
        x: gamingPage.x + gamingPage.width + 8
        y: gamingPage.y
        height: gamingPage.height
    }

    ScreensaversPage {
        id: screensaversPage
        filePicker: win.filePicker
        visible: win.tab.kind === "screensavers" && win.backend.problem === ""
        x: 30
        y: 84
        width: win.width - 78
        height: win.height - 104 - statusBar.height
        backend: win.backend
        cropAtStart: win.cropOpen
    }
    ScrollRail {
        objectName: "screensaversPageScrollRail"
        flickable: screensaversPage
        active: screensaversPage.visible
        x: screensaversPage.x + screensaversPage.width + 8
        y: screensaversPage.y
        height: screensaversPage.height
    }

    // ---- the status bar: does the module follow, and what was done
    Rectangle {
        id: statusBar
        x: 0
        y: win.height - height
        width: win.width
        height: 52
        color: "#15171a"
        Rectangle {
            width: parent.width
            height: 1
            color: "#33ffffff"
        }
        Row {
            x: 20
            width: Math.max(120, statusBar.width - x - (updateButton.visible ? updateButton.width + 32 : 20)
                            - (win.message !== "" ? statusBar.width / 2.4 : 0))
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -8
            spacing: 8
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                radius: 5
                color: win.daemon.running ? "#43a047" : "#ffb74d"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - 18 - (startDaemon.visible ? startDaemon.width + 8 : 0))
                elide: Text.ElideRight
                color: "#c9ced6"
                font.pixelSize: 13
                text: win.daemon.running ? qsTr("galleond is running: a change reaches the module at once.")
                                         : qsTr("galleond is not running: changes are saved and will show when it starts.")
            }
            Pill {
                id: startDaemon
                visible: !win.daemon.running
                height: 24
                text: qsTr("Start it")
                onClicked: win.answered(win.daemon.start(), qsTr("Starting galleond…"))
            }
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: updateButton.visible ? updateButton.width + 28 : 20
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -8
            width: parent.width / 2.4 - (updateButton.visible ? updateButton.width + 8 : 0)
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: win.messageIsError ? "#ff8a80" : "#9be7a0"
            font.pixelSize: 13
            text: win.message
        }
        Text {
            objectName: "firmwareStatusBar"
            x: 38
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            width: parent.width - 58
            font.pixelSize: 11
            textFormat: Text.PlainText
            elide: Text.ElideRight
            color: win.backend.homeWidgets.firmwareWarning ? "#ffb74d" : "#9aa0a6"
            text: qsTr("Firmware Stream Deck : %1").arg((win.daemon.running || win.backend.homeWidgets.firmwareSimulation) && win.backend.homeWidgets.firmwareVersion
                  ? win.backend.homeWidgets.firmwareVersion : qsTr("inconnu"))
                  + (win.backend.homeWidgets.firmwareSimulation ? qsTr(" · SIMULATION") : "")
                  + ((win.daemon.running || win.backend.homeWidgets.firmwareSimulation) && win.backend.homeWidgets.firmwareWarning ? qsTr(" · Mise à jour recommandée (onglet Clavier)") : "")
        }
        Pill {
            id: updateButton
            objectName: "updatesButton"
            visible: win.updates !== null && !win.updates.packageManaged
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -8
            height: 26
            selected: win.updates !== null && win.updates.available
            text: win.updates !== null && win.updates.available ? qsTr("Update %1").arg(win.updates.latestVersion) : qsTr("Updates")
            onClicked: win.updatesOpen = true
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 99
        visible: languageMenu.expanded
        onClicked: languageMenu.expanded = false
    }
    // Always reachable; language names remain recognizable after an accidental change.
    Rectangle {
        id: languageMenu
        objectName: "languageMenu"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 176
        height: 32
        radius: 6
        color: "#2e3238"
        visible: win.languages !== null
        property bool expanded: false
        function choose(code) {
            win.message = ""
            expanded = false
            forceActiveFocus()
            // Retranslation rebuilds the option delegates; change language last.
            win.languages.preference = code
        }
        activeFocusOnTab: true
        border.width: activeFocus ? 2 : 0
        border.color: "#80e5d1"
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                expanded = !expanded
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                expanded = false
                event.accepted = true
            }
        }
        Accessible.role: Accessible.ComboBox
        Accessible.name: qsTr("Language")
        Accessible.onPressAction: { forceActiveFocus(); expanded = !expanded }
        Text {
            anchors.centerIn: parent
            color: "white"
            text: "🌐 " + (win.languages && win.languages.preference === "system" ? qsTr("System language")
                          : win.languages && win.languages.language === "fr" ? "Français"
                          : win.languages && win.languages.language === "de" ? "Deutsch"
                          : win.languages && win.languages.language === "es" ? "Español" : "English") + " ▾"
            font.pixelSize: 12
        }
        MouseArea { anchors.fill: parent; onClicked: { languageMenu.forceActiveFocus(); languageMenu.expanded = !languageMenu.expanded } }
        Column {
            anchors.top: parent.bottom
            anchors.right: parent.right
            visible: languageMenu.expanded
            Repeater {
                model: [{code: "system", name: qsTr("System language")}, {code: "en", name: "English"}, {code: "fr", name: "Français"}, {code: "de", name: "Deutsch"}, {code: "es", name: "Español"}]
                delegate: Rectangle {
                    id: languageOption
                    required property var modelData
                    objectName: "languageOption_" + modelData.code
                    width: 180
                    height: 36
                    color: activeFocus ? "#3a3f47" : "#2e3238"
                    activeFocusOnTab: visible
                    Keys.onReturnPressed: languageMenu.choose(modelData.code)
                    Keys.onSpacePressed: languageMenu.choose(modelData.code)
                    Keys.onEscapePressed: { languageMenu.expanded = false; languageMenu.forceActiveFocus() }
                    Accessible.role: Accessible.RadioButton
                    Accessible.checkable: true
                    Accessible.checked: win.languages !== null && win.languages.preference === modelData.code
                    Accessible.name: modelData.name
                    Accessible.onPressAction: languageMenu.choose(modelData.code)
                    Text { anchors.centerIn: parent; color: "white"; text: (win.languages !== null && win.languages.preference === languageOption.modelData.code ? "✓ " : "") + languageOption.modelData.name }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            languageMenu.choose(languageOption.modelData.code)
                        }
                    }
                }
            }
        }
        z: 100
    }

    UpdatePanel {
        anchors.fill: parent
        visible: win.updatesOpen && win.updates !== null
        updates: win.updates
        onClosed: win.updatesOpen = false
    }
}

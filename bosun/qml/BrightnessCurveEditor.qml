// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The brightness of the keyboard through the day: time of day on X (0 to 24 h), 0 to 100 % on Y. A smooth, closed
// curve is drawn through 24 points, one per hour, that are dragged up and down; each one is brought to the nearest
// step of 10 % (100 in the keyboard's unit) as it is dragged. Nothing is sent while dragging: `committed` fires once
// per point, when the pointer is released or a key press has rested. The point nearest to the time of day, the one
// galleond applies now, is marked. QML draws and forwards; the curve, the steps and the file are the model's
// (BrightnessCurveModel), the write to the keyboard is galleond's.
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: editor

    // The BrightnessCurveModel: hours (24 values, 0 to 1000), smoothWith(), snap(), pointForTime().
    required property var curveModel
    property bool enabledLook: true
    // The hour being dragged or moved by the keys, and the value it would take (-1: nothing).
    property int previewHour: -1
    property int previewValue: -1
    // The hour of the point nearest to the time of day.
    property int nowPoint: 0
    property string nowText: ""
    signal committed(int hour, int value)

    readonly property real plotLeft: 44
    readonly property real plotTop: 14
    readonly property real plotWidth: width - plotLeft - 16
    readonly property real plotHeight: height - plotTop - 30
    readonly property int steps: 10

    implicitHeight: 250
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Keyboard brightness through the day")

    function xFor(hour) { return plotLeft + hour / 24 * plotWidth }
    function yFor(value) { return plotTop + plotHeight * (1 - value / 1000) }
    function valueAtY(y) { return editor.curveModel.snap((1 - (y - plotTop) / plotHeight) * 1000) }
    function shownValue(hour) { return hour === previewHour && previewValue >= 0 ? previewValue : editor.curveModel.hours[hour] }

    function choose(hour, value) {
        previewHour = hour
        previewValue = editor.curveModel.snap(value)
        curve.requestPaint()
    }
    function commit() {
        const hour = previewHour
        const value = previewValue
        previewHour = -1
        previewValue = -1
        if (hour >= 0 && value >= 0 && value !== editor.curveModel.hours[hour])
            editor.committed(hour, value)
        else
            curve.requestPaint()
    }
    function refreshNow() {
        const now = new Date()
        nowPoint = editor.curveModel.pointForTime(now.getHours(), now.getMinutes())
        nowText = Qt.formatTime(now, "hh:mm")
    }

    Component.onCompleted: refreshNow()
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: { editor.refreshNow(); curve.requestPaint() }
    }
    Timer {
        id: rest
        interval: 450
        onTriggered: editor.commit()
    }
    Connections {
        target: editor.curveModel
        function onChanged() { curve.requestPaint() }
    }
    onWidthChanged: curve.requestPaint()
    onHeightChanged: curve.requestPaint()

    // The grid and the curve.
    Canvas {
        id: curve
        objectName: "brightnessCurveCanvas"
        anchors.fill: parent
        renderTarget: Canvas.Image
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.lineWidth = 1
            ctx.font = "12px sans-serif"
            ctx.fillStyle = "#7d8591"
            ctx.strokeStyle = "#2e3238"
            // 0 to 100 % every 20 %.
            for (let percent = 0; percent <= 100; percent += 20) {
                const y = Math.round(editor.yFor(percent * 10)) + 0.5
                ctx.beginPath(); ctx.moveTo(editor.plotLeft, y); ctx.lineTo(editor.plotLeft + editor.plotWidth, y); ctx.stroke()
                ctx.textAlign = "right"
                ctx.fillText(percent + " %", editor.plotLeft - 8, y + 4)
            }
            // The hours, every three.
            for (let hour = 0; hour <= 24; hour += 3) {
                const x = Math.round(editor.xFor(hour)) + 0.5
                ctx.beginPath(); ctx.moveTo(x, editor.plotTop); ctx.lineTo(x, editor.plotTop + editor.plotHeight); ctx.stroke()
                ctx.textAlign = "center"
                ctx.fillText(hour + " h", x, editor.plotTop + editor.plotHeight + 18)
            }
            // The time of day.
            const now = new Date()
            const nowX = editor.xFor(now.getHours() + now.getMinutes() / 60)
            ctx.strokeStyle = "#64b5f6"
            ctx.setLineDash([4, 4])
            ctx.beginPath(); ctx.moveTo(nowX, editor.plotTop); ctx.lineTo(nowX, editor.plotTop + editor.plotHeight); ctx.stroke()
            ctx.setLineDash([])
            // The smooth curve, closed on 24 h (the point at 24 h is the one at 0 h), with the point being dragged.
            const sample = 0.1
            ctx.beginPath()
            for (let hour = 0; hour <= 24.0001; hour += sample) {
                const value = editor.curveModel.smoothWith(hour, editor.previewHour, editor.previewValue)
                const x = editor.xFor(hour)
                const y = editor.yFor(value)
                if (hour === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.strokeStyle = editor.enabledLook ? "#a5d6a7" : "#5a6068"
            ctx.lineWidth = 3
            ctx.stroke()
        }
    }

    // One handle per hour, dragged up and down.
    Repeater {
        id: points
        model: 24
        Item {
            id: handle
            required property int index
            readonly property int value: editor.shownValue(index)
            readonly property bool dragging: editor.previewHour === index
            x: editor.xFor(index) - width / 2
            y: editor.yFor(value) - height / 2
            width: 26
            height: 26
            activeFocusOnTab: editor.enabledLook && index === 0 // Tab enters the points once; the arrows move among them
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Brightness at %1 h").arg(index)
            Accessible.description: qsTr("%1 %").arg(value / 10)
            onActiveFocusChanged: if (activeFocus) editor.choose(index, value)

            // Up and Down change the point (committed once the key has rested); Left and Right move to the
            // neighbouring hour.
            Keys.onPressed: event => {
                if (!editor.enabledLook)
                    return
                const current = editor.previewHour === index ? editor.previewValue : value
                let handled = true
                if (event.key === Qt.Key_Up) {
                    editor.choose(index, Math.min(1000, current + 100)); rest.restart()
                } else if (event.key === Qt.Key_Down) {
                    editor.choose(index, Math.max(0, current - 100)); rest.restart()
                } else if (event.key === Qt.Key_Right) {
                    rest.stop(); editor.commit(); points.itemAt((index + 1) % 24).forceActiveFocus()
                } else if (event.key === Qt.Key_Left) {
                    rest.stop(); editor.commit(); points.itemAt((index + 23) % 24).forceActiveFocus()
                } else {
                    handled = false
                }
                event.accepted = handled
            }

            Rectangle {
                anchors.centerIn: parent
                width: handle.dragging || handle.activeFocus ? 18 : 12
                height: width
                radius: width / 2
                color: !editor.enabledLook ? "#8a8f98" : handle.index === editor.nowPoint ? "#64b5f6" : "white"
                border.width: handle.activeFocus ? 3 : 2
                border.color: handle.activeFocus ? "#64b5f6" : "#2e7d32"
            }
            // The figure over the point being dragged.
            Text {
                visible: handle.dragging || handle.activeFocus
                anchors.horizontalCenter: parent.horizontalCenter
                y: handle.value > 800 ? parent.height + 2 : -height - 2
                color: "white"
                font.pixelSize: 13
                font.bold: true
                text: handle.value / 10 + " %"
            }
            MouseArea {
                anchors.fill: parent
                enabled: editor.enabledLook
                preventStealing: true
                cursorShape: Qt.SizeVerCursor
                // The pointer is read in the editor's own frame: the handle moves while it is dragged.
                onPressed: { handle.forceActiveFocus(); rest.stop(); editor.choose(handle.index, editor.valueAtY(mapToItem(editor, mouseX, mouseY).y)) }
                onPositionChanged: if (pressed) editor.choose(handle.index, editor.valueAtY(mapToItem(editor, mouseX, mouseY).y))
                onReleased: editor.commit()
                onCanceled: { editor.previewHour = -1; editor.previewValue = -1; curve.requestPaint() }
            }
        }
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: -2
        color: "#64b5f6"
        font.pixelSize: 12
        text: qsTr("now %1 · point %2 h: %3 %").arg(editor.nowText).arg(editor.nowPoint).arg(editor.curveModel.hours[editor.nowPoint] / 10)
    }
}

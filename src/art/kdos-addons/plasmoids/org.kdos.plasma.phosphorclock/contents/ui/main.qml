/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Phosphor Clock: the time in the fixed-width face.
 *
 * In a panel it is the time, and the date under it when asked for; on the
 * desktop and in its popup it is a large time over the date, the ISO week
 * and UTC. The digits are drawn in the theme's text colour, which every KDOS
 * scheme holds legible on the panel, and the separator in the accent. The
 * separator blinks only while animations run: with reduced motion
 * (Kirigami.Units.longDuration 0) it stays lit.
 *
 * The clock ticks on a one-second timer re-aligned to the wall clock at
 * every tick, so a suspend, a clock change or a slow frame never leaves it a
 * second behind for long.
 *
 * SPDX-License-Identifier: MIT
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: root

    readonly property bool showSeconds: Plasmoid.configuration.showSeconds
    readonly property bool use24Hour: Plasmoid.configuration.use24Hour
    readonly property bool showDate: Plasmoid.configuration.showDate
    readonly property bool blinks: Plasmoid.configuration.blinkSeparator && Kirigami.Units.longDuration > 0
    readonly property bool inPanel: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
        || Plasmoid.formFactor === PlasmaCore.Types.Vertical

    property date now: new Date()
    property bool separatorLit: true

    function pad(n) {
        return n < 10 ? "0" + n : "" + n;
    }

    function hoursText(d) {
        if (use24Hour) {
            return pad(d.getHours());
        }
        const h = d.getHours() % 12;
        return pad(h === 0 ? 12 : h);
    }

    function suffix(d) {
        return use24Hour ? "" : (d.getHours() < 12 ? Qt.locale().amText : Qt.locale().pmText);
    }

    // ISO 8601: the week holding the year's first Thursday is week 1.
    function isoWeek(d) {
        const t = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
        const day = t.getUTCDay() || 7;
        t.setUTCDate(t.getUTCDate() + 4 - day);
        const start = new Date(Date.UTC(t.getUTCFullYear(), 0, 1));
        return Math.ceil(((t - start) / 86400000 + 1) / 7);
    }

    function utcText(d) {
        return pad(d.getUTCHours()) + ":" + pad(d.getUTCMinutes());
    }

    readonly property string spokenTime: Qt.formatTime(now, Qt.locale().timeFormat(Locale.ShortFormat))
    readonly property string longDate: Qt.formatDate(now, Qt.locale().dateFormat(Locale.LongFormat))

    toolTipMainText: spokenTime
    toolTipSubText: longDate
    preferredRepresentation: inPanel ? compactRepresentation : fullRepresentation

    Timer {
        id: tick
        repeat: false
        running: true
        triggeredOnStart: true
        onTriggered: {
            root.now = new Date();
            root.separatorLit = root.blinks ? !root.separatorLit : true;
            interval = 1000 - root.now.getMilliseconds();
            start();
        }
    }

    component Digits: PlasmaComponents3.Label {
        property real size: Kirigami.Theme.defaultFont.pixelSize
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pixelSize: size
        color: Kirigami.Theme.textColor
        Accessible.ignored: true
    }

    component Separator: PlasmaComponents3.Label {
        property real size: Kirigami.Theme.defaultFont.pixelSize
        text: ":"
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pixelSize: size
        color: Kirigami.Theme.highlightColor
        opacity: root.separatorLit ? 1 : 0.25
        Accessible.ignored: true
    }

    component Time: RowLayout {
        id: time
        property real pixelSize: Kirigami.Theme.defaultFont.pixelSize
        spacing: 0

        Accessible.role: Accessible.StaticText
        Accessible.name: root.spokenTime

        Digits {
            size: time.pixelSize
            text: root.hoursText(root.now)
        }
        Separator {
            size: time.pixelSize
        }
        Digits {
            size: time.pixelSize
            text: root.pad(root.now.getMinutes())
        }
        Separator {
            size: time.pixelSize
            visible: root.showSeconds
        }
        Digits {
            size: time.pixelSize
            visible: root.showSeconds
            text: root.pad(root.now.getSeconds())
        }
        Digits {
            size: time.pixelSize * 0.5
            visible: text.length > 0
            text: root.suffix(root.now).length > 0 ? " " + root.suffix(root.now) : ""
            Layout.alignment: Qt.AlignBaseline
        }
    }

    compactRepresentation: MouseArea {
        id: compact

        Layout.minimumWidth: root.inPanel && Plasmoid.formFactor === PlasmaCore.Types.Horizontal
            ? column.implicitWidth : Kirigami.Units.gridUnit
        Layout.minimumHeight: Plasmoid.formFactor === PlasmaCore.Types.Vertical
            ? column.implicitHeight : Kirigami.Units.gridUnit

        activeFocusOnTab: true
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: i18nc("@info:tooltip accessible", "%1, %2", root.spokenTime, root.longDate)
        Accessible.description: i18nc("@info accessible", "Shows the clock's popup")
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.expanded = !root.expanded;
                event.accepted = true;
            }
        }
        onClicked: root.expanded = !root.expanded

        ColumnLayout {
            id: column
            anchors.centerIn: parent
            spacing: 0

            Time {
                Layout.alignment: Qt.AlignHCenter
                pixelSize: root.showDate
                    ? Math.min(compact.height * 0.5, Kirigami.Theme.defaultFont.pixelSize * 1.1)
                    : Math.min(compact.height * 0.6, Kirigami.Theme.defaultFont.pixelSize * 1.4)
            }
            PlasmaComponents3.Label {
                Layout.alignment: Qt.AlignHCenter
                visible: root.showDate
                text: Qt.formatDate(root.now, Qt.locale().dateFormat(Locale.ShortFormat))
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                Accessible.ignored: true
            }
        }
    }

    fullRepresentation: ColumnLayout {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 14
        Layout.minimumHeight: Kirigami.Units.gridUnit * 8
        spacing: Kirigami.Units.smallSpacing

        Time {
            Layout.alignment: Qt.AlignHCenter
            pixelSize: Kirigami.Theme.defaultFont.pixelSize * 4
        }
        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.longDate
            font.family: Kirigami.Theme.fixedWidthFont.family
            Accessible.name: text
        }
        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18nc("@info ISO week number, then the time in UTC", "Week %1 · %2 UTC",
                        root.isoWeek(root.now), root.utcText(root.now))
            font.family: Kirigami.Theme.fixedWidthFont.family
            color: Kirigami.Theme.disabledTextColor
            Accessible.name: i18nc("@info accessible", "Week %1, %2 coordinated universal time",
                                   root.isoWeek(root.now), root.utcText(root.now))
        }
    }
}

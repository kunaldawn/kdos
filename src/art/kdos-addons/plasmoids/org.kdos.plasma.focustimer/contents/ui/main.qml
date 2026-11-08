/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Focus Timer: work for a set time, then rest.
 *
 * Focus periods alternate with short breaks, and every longBreakEvery-th
 * break is a long one. Each change of period posts a notification through
 * the kdos-addons notifyrc, so the Notifications page can silence it or give
 * it a sound like any other event.
 *
 * The time left is computed from the period's end on the wall clock, never
 * counted down tick by tick: a timer that only decremented would fall behind
 * by every late tick and by the whole of a suspend. A period that ended while
 * the machine slept is over when it wakes.
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
import org.kde.notification

PlasmoidItem {
    id: root

    readonly property int focusPhase: 0
    readonly property int shortBreakPhase: 1
    readonly property int longBreakPhase: 2

    property int phase: focusPhase
    property bool running: false
    property int completed: 0
    property real endsAt: 0
    property int remaining: periodSeconds(phase)

    readonly property int total: periodSeconds(phase)
    readonly property bool inPanel: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
        || Plasmoid.formFactor === PlasmaCore.Types.Vertical

    function periodSeconds(p) {
        const c = Plasmoid.configuration;
        if (p === root.shortBreakPhase) {
            return c.shortBreakMinutes * 60;
        }
        if (p === root.longBreakPhase) {
            return c.longBreakMinutes * 60;
        }
        return c.focusMinutes * 60;
    }

    function phaseName(p) {
        if (p === root.shortBreakPhase) {
            return i18nc("@info:status the timer's period", "Short break");
        }
        if (p === root.longBreakPhase) {
            return i18nc("@info:status the timer's period", "Long break");
        }
        return i18nc("@info:status the timer's period", "Focus");
    }

    function clock(seconds) {
        const m = Math.floor(seconds / 60);
        const s = seconds % 60;
        return (m < 10 ? "0" + m : "" + m) + ":" + (s < 10 ? "0" + s : "" + s);
    }

    function start() {
        endsAt = Date.now() + remaining * 1000;
        running = true;
    }

    function pause() {
        update();
        running = false;
    }

    function reset() {
        running = false;
        phase = root.focusPhase;
        completed = 0;
        remaining = periodSeconds(phase);
    }

    function skip() {
        advance(false);
    }

    function update() {
        if (running) {
            remaining = Math.max(0, Math.ceil((endsAt - Date.now()) / 1000));
        }
    }

    // The period is over: say so, and move to the next one.
    function advance(announce) {
        const ended = phase;
        if (ended === root.focusPhase) {
            completed += 1;
            phase = completed % Plasmoid.configuration.longBreakEvery === 0
                ? root.longBreakPhase : root.shortBreakPhase;
        } else {
            phase = root.focusPhase;
        }
        remaining = periodSeconds(phase);
        running = false;
        if (announce) {
            const notice = phaseNotice.createObject(root, {
                title: ended === root.focusPhase
                    ? i18nc("@title notification", "Time for a break")
                    : i18nc("@title notification", "Back to focus"),
                text: i18nc("@info notification, %1 is the next period, %2 its length",
                            "%1 next, %2.", phaseName(phase), clock(remaining)),
            }) as Notification;
            notice.sendEvent();
        }
        if (Plasmoid.configuration.autoStartNext) {
            start();
        }
    }

    Component {
        id: phaseNotice
        Notification {
            componentName: "kdos-addons"
            eventId: "phaseEnded"
            iconName: "chronometer"
        }
    }

    Timer {
        interval: 250
        repeat: true
        running: root.running
        onTriggered: {
            root.update();
            if (root.remaining === 0) {
                root.advance(true);
            }
        }
    }

    Connections {
        target: Plasmoid.configuration
        function onValueChanged() {
            if (!root.running) {
                root.remaining = root.periodSeconds(root.phase);
            }
        }
    }

    toolTipMainText: i18nc("@info:tooltip %1 is the period, %2 the time left", "%1: %2",
                           phaseName(phase), clock(remaining))
    toolTipSubText: running
        ? i18ncp("@info:tooltip", "%1 focus period done", "%1 focus periods done", completed)
        : i18nc("@info:tooltip", "Paused")
    preferredRepresentation: inPanel ? compactRepresentation : fullRepresentation
    Plasmoid.icon: "chronometer"
    Plasmoid.status: running ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: root.running ? i18nc("@action", "Pause") : i18nc("@action", "Start")
            icon.name: root.running ? "media-playback-pause" : "media-playback-start"
            onTriggered: root.running ? root.pause() : root.start()
        },
        PlasmaCore.Action {
            text: i18nc("@action", "Skip to the Next Period")
            icon.name: "media-skip-forward"
            onTriggered: root.skip()
        },
        PlasmaCore.Action {
            text: i18nc("@action", "Reset")
            icon.name: "edit-reset"
            onTriggered: root.reset()
        }
    ]

    compactRepresentation: MouseArea {
        id: compact

        Layout.minimumWidth: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
            ? row.implicitWidth : Kirigami.Units.iconSizes.small
        Layout.minimumHeight: Kirigami.Units.iconSizes.small

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: root.toolTipMainText
        Accessible.description: i18nc("@info accessible", "Shows the focus timer")
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                root.expanded = !root.expanded;
                event.accepted = true;
            }
        }
        onClicked: root.expanded = !root.expanded

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "chronometer"
                Layout.preferredWidth: Math.min(compact.height, Kirigami.Units.iconSizes.medium)
                Layout.preferredHeight: Layout.preferredWidth
                Accessible.ignored: true
            }
            PlasmaComponents3.Label {
                visible: root.running && Plasmoid.formFactor === PlasmaCore.Types.Horizontal
                text: root.clock(root.remaining)
                font.family: Kirigami.Theme.fixedWidthFont.family
                Accessible.ignored: true
            }
        }
    }

    fullRepresentation: ColumnLayout {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 14
        Layout.minimumHeight: Kirigami.Units.gridUnit * 12
        spacing: Kirigami.Units.largeSpacing

        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.phaseName(root.phase)
            font.bold: true
            Accessible.name: text
        }
        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.clock(root.remaining)
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 4
            Accessible.role: Accessible.StaticText
            Accessible.name: i18ncp("@info accessible", "%1 minute left", "%1 minutes left",
                                    Math.ceil(root.remaining / 60))
        }
        PlasmaComponents3.ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: root.total
            value: root.total - root.remaining
            Accessible.name: i18nc("@info accessible", "Progress through the %1 period",
                                   root.phaseName(root.phase))
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Button {
                text: root.running ? i18nc("@action:button", "Pause") : i18nc("@action:button", "Start")
                icon.name: root.running ? "media-playback-pause" : "media-playback-start"
                Accessible.name: text
                onClicked: root.running ? root.pause() : root.start()
            }
            PlasmaComponents3.Button {
                text: i18nc("@action:button", "Skip")
                icon.name: "media-skip-forward"
                Accessible.name: i18nc("@action:button accessible", "Skip to the next period")
                onClicked: root.skip()
            }
            PlasmaComponents3.Button {
                text: i18nc("@action:button", "Reset")
                icon.name: "edit-reset"
                Accessible.name: i18nc("@action:button accessible", "Reset the timer")
                onClicked: root.reset()
            }
        }
        PlasmaComponents3.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18ncp("@info", "%1 focus period done", "%1 focus periods done", root.completed)
            color: Kirigami.Theme.disabledTextColor
            Accessible.name: text
        }
    }
}

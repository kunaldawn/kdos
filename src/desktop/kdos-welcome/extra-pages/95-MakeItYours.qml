/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   95-MakeItYours.qml — whole sets of programs, prebuilt, the tour's last page
 *
 * The base system carries the desktop and a few applications; everything
 * else KDOS ports is in the release's binhost. This page lists the groups of
 * /ports/groups as `kdos update groups --json` reports them (installed, how
 * many members are missing, what installing the rest downloads), lets the
 * person tick some, and installs them one at a time through
 * `pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <id>`:
 * Discover's prebuilt helper, whose `kdos update install --prebuilt` takes a
 * group id as well as a port, under the polkit action io.kdos.ports.install
 * (the password is asked once and kept for the session's next ones). The
 * rules (which group is installable, the queue) are kdos/groups.js, which
 * tests/tst_groups.qml tests.
 *
 * A GROUP WITH NO SIZE IS NOT OFFERED. The size is null when the signed
 * binhost index is not in /var/lib/kdos/binhost, or when some package of the
 * group is not prebuilt, and a prebuilt install refuses both. Fetching the
 * index needs root and this page has no root path but the helper's, so with
 * no index at all the page says to connect and press Check for Updates in
 * Discover, which fetches it, and offers to look again. A group that is not
 * all prebuilt is left to Discover, which can build it here.
 *
 * On the live medium an install goes into the session's memory and is gone
 * at the next start, and the page says so.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import "kdos" as Kdos
import "kdos/groups.js" as Groups

Welcome.ScrollablePage {
    id: page

    heading: i18ndc("kdos-welcome", "@title:window", "Make It Yours")
    description: i18ndc("kdos-welcome", "@info:usagetip", "KDOS starts small. Add whole sets of programs here, downloaded ready to run; Discover has every program one at a time.")

    Kdos.Run {
        id: run
    }

    readonly property bool live: Qt.application.arguments.indexOf("--live-environment") >= 0
    property var groups: []
    property bool loaded: false
    property var chosen: []
    // The install in progress, Groups.start()'s shape, or null.
    property var job: null
    // The last install's outcome: Groups.step()'s final state, or null.
    property var outcome: null

    readonly property bool noIndex: Groups.noIndex(page.groups)
    readonly property bool installing: page.job !== null
    readonly property real downloadBytes: Groups.bytes(page.groups, page.chosen)

    function refresh(): void {
        run.readJson(["kdos", "update", "groups", "--json"], (code, value) => {
            page.groups = Groups.rows(value);
            page.loaded = true;
            page.chosen = page.chosen.filter(id => page.groups.some(g => g.id === id && Groups.installable(g)));
        });
    }

    function toggle(id: string, on: bool): void {
        const rest = page.chosen.filter(c => c !== id);
        page.chosen = on ? rest.concat([id]) : rest;
    }

    function groupTitle(id: string): string {
        const g = page.groups.find(r => r.id === id);
        return g ? g.title : id;
    }

    function install(): void {
        page.outcome = null;
        page.job = Groups.start(page.chosen);
        page.next();
    }

    function next(): void {
        if (page.job.current.length === 0) {
            page.outcome = page.job;
            page.job = null;
            page.chosen = [];
            page.refresh();
            return;
        }
        run.run(["pkexec", "/usr/libexec/kdos-discover/kdos-ports-helper", "install", page.job.current], (code, text) => {
            page.job = Groups.step(page.job, code, text);
            page.next();
        });
    }

    function sizeText(g: var): string {
        if (g.installed) {
            return i18ndc("kdos-welcome", "@info", "Installed");
        }
        if (g.size === null) {
            return page.noIndex
                ? i18ndc("kdos-welcome", "@info", "Size not known until the package list is on this computer")
                : i18ndc("kdos-welcome", "@info", "Not all of it is ready-made; Discover can build it on this computer");
        }
        return i18ndcp("kdos-welcome", "@info %2 is a size", "%1 package, %2 to download", "%1 packages, %2 to download",
                       g.packages, page.formatSize(g.size));
    }

    // A size as a person reads it, in binary units.
    function formatSize(bytes: real): string {
        const units = ["B", "KiB", "MiB", "GiB", "TiB"];
        let v = bytes;
        let u = 0;
        while (v >= 1024 && u < units.length - 1) {
            v /= 1024;
            u++;
        }
        return i18ndc("kdos-welcome", "@info %1 number %2 unit", "%1 %2", u === 0 ? v.toString() : v.toFixed(1), units[u]);
    }

    Component.onCompleted: refresh()

    view: Kdos.Flick {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: page.live
            text: i18ndc("kdos-welcome", "@info", "This is the live session: what you install here runs from memory and is gone when the machine restarts. The installer offers the same groups for the installed system.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: page.loaded && page.noIndex
            text: i18ndc("kdos-welcome", "@info", "This computer does not have the list of ready-made packages yet. Connect to the internet, open Discover and press Check for Updates, then look again here.")
            actions: [
                Kirigami.Action {
                    text: i18ndc("kdos-welcome", "@action:button", "Open Discover")
                    icon.name: "plasmadiscover"
                    onTriggered: Welcome.Utils.launchApp("org.kde.discover")
                },
                Kirigami.Action {
                    text: i18ndc("kdos-welcome", "@action:button", "Look Again")
                    icon.name: "view-refresh"
                    enabled: !page.installing
                    onTriggered: page.refresh()
                }
            ]
        }

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            visible: page.loaded && page.groups.length === 0
            icon.name: "package-x-generic"
            text: i18ndc("kdos-welcome", "@info:placeholder", "This system names no groups of programs")
            explanation: i18ndc("kdos-welcome", "@info", "Discover has every program KDOS offers, one at a time.")
        }

        Repeater {
            model: page.groups
            delegate: QQC2.CheckDelegate {
                id: row

                required property var modelData

                Layout.fillWidth: true
                enabled: !page.installing && Groups.installable(row.modelData)
                checked: row.modelData.installed || page.chosen.indexOf(row.modelData.id) >= 0
                text: row.modelData.title
                Accessible.name: i18ndc("kdos-welcome", "@info:whatsthis %1 group %2 its size or state", "%1: %2", row.modelData.title, page.sizeText(row.modelData))
                onToggled: page.toggle(row.modelData.id, checked)

                contentItem: ColumnLayout {
                    spacing: 0
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.text
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.sizeText(row.modelData)
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        QQC2.Button {
            visible: page.groups.some(g => Groups.installable(g))
            enabled: !page.installing && page.chosen.length > 0
            icon.name: "download"
            text: page.chosen.length === 0
                ? i18ndc("kdos-welcome", "@action:button", "Install")
                : i18ndcp("kdos-welcome", "@action:button %2 is a size", "Install %1 Group (%2)", "Install %1 Groups (%2)",
                          page.chosen.length, page.formatSize(page.downloadBytes))
            Accessible.name: text
            onClicked: page.install()
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: page.installing
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                id: progressText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: page.job === null ? ""
                    : i18ndc("kdos-welcome", "@info:status %1 group %2 its number %3 how many", "Installing %1 (%2 of %3)…",
                             page.groupTitle(page.job.current), page.job.total - page.job.queue.length + 1, page.job.total)
                Accessible.name: text
            }
            QQC2.ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: page.job === null ? 1 : page.job.total
                value: page.job === null ? 0 : page.job.total - page.job.queue.length
                Accessible.name: progressText.text
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Positive
            visible: page.outcome !== null && page.outcome.done.length > 0
            text: page.outcome === null ? ""
                : i18ndc("kdos-welcome", "@info %1 a list of groups", "Installed: %1. The programs are in the application menu.",
                         page.outcome.done.map(id => page.groupTitle(id)).join(", "))
        }

        Repeater {
            model: page.outcome === null ? [] : page.outcome.failed
            delegate: Kirigami.InlineMessage {
                required property var modelData

                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: true
                text: i18ndc("kdos-welcome", "@info %1 group %2 what went wrong", "%1 was not installed: %2", page.groupTitle(modelData.id), modelData.text)
            }
        }

        Kdos.Point {
            icon: "plasmadiscover"
            title: i18ndc("kdos-welcome", "@title", "Everything else")
            text: i18ndc("kdos-welcome", "@info", "Discover has every program on its own, ready-made or built on this computer, and the boxed applications from other distributions.")
            action: i18ndc("kdos-welcome", "@action:button", "Open Discover")
            actionIcon: "plasmadiscover"
            onTriggered: Welcome.Utils.launchApp("org.kde.discover")
        }
    }
}

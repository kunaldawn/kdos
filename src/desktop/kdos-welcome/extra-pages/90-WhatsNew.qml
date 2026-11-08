/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   90-WhatsNew.qml — the release notes of the slot just started
 *
 * Never part of the tour (`show` is false); it is opened once after a new
 * slot has been confirmed, as `plasma-welcome --pages 90-WhatsNew.qml`,
 * which shows a page whatever its `show` says. The notes are `kdos update
 * notes --json`: the version now running, the one before it, and the
 * release notes as Markdown, all read from the slot itself, so they are
 * there with no network.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import "kdos" as Kdos

Welcome.ScrollablePage {
    id: page

    show: false

    heading: notes !== null && typeof notes.version === "string"
        ? i18ndc("kdos-welcome", "@title:window %1 is a KDOS version", "What's New in KDOS %1", notes.version)
        : i18ndc("kdos-welcome", "@title:window", "What's New")
    description: notes !== null && typeof notes.previous === "string" && notes.previous.length > 0
        ? i18ndc("kdos-welcome", "@info:usagetip %1 is the previous version", "KDOS was updated from %1. If something you rely on misbehaves, System Settings → System Versions starts the previous version again.", notes.previous)
        : i18ndc("kdos-welcome", "@info:usagetip", "KDOS was updated. If something you rely on misbehaves, System Settings → System Versions starts the previous version again.")

    Kdos.Run {
        id: run
    }

    property var notes: null
    property bool failed: false

    Component.onCompleted: run.json(["kdos", "update", "notes", "--json"], (code, value) => {
        page.notes = value;
        page.failed = code !== 0 || value === null;
    })

    view: Kdos.Flick {
        QQC2.Label {
            Layout.fillWidth: true
            visible: !page.failed && page.notes !== null
            wrapMode: Text.WordWrap
            textFormat: Text.MarkdownText
            text: page.notes !== null && typeof page.notes.notes === "string" ? page.notes.notes : ""
            onLinkActivated: link => Qt.openUrlExternally(link)
        }

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            visible: page.failed
            icon.name: "documentinfo"
            text: i18ndc("kdos-welcome", "@info:placeholder", "No release notes came with this version")
        }

        QQC2.Button {
            text: i18ndc("kdos-welcome", "@action:button", "System Versions…")
            icon.name: "preferences-system"
            onClicked: run.run(["kcmshell6", "kcm_kdos_system"], () => {})
            Accessible.name: text
        }
    }
}

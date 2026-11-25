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
 * Never part of the tour (`show` is false); it is opened as `plasma-welcome
 * --pages 90-WhatsNew.qml`, which shows a page whatever its `show` says: by
 * kdos-notice after a confirmed login on a version other than the one this
 * account recorded (an update, or going back by hand from a version it used),
 * never on an account's first login or on the live medium; and at any time
 * Welcome Center is not running, by the Release Notes entry
 * (io.kdos.releasenotes.desktop). The notes are `kdos update notes --json`:
 * the version now running, the other slot's when this boot came from it or
 * went back from it, whether it went back, whether this is the live medium,
 * and the release notes as Markdown, all read from the slot itself, so they
 * are there with no network.
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
    // Read from `kdos update notes --json`: `went_back` (this
    // slot is the older one, and the later version in the other slot did not
    // start or was not kept), `previous` (the version this boot was updated
    // from), `live` (the live medium, which has no slots, so the page names
    // the running version alone and offers no System Versions), and neither —
    // a fresh install, where no update has run and the page claims none.
    // Notes that could not be read leave the System Versions pointer alone.
    description: {
        if (notes !== null && notes.went_back === true && typeof notes.previous === "string" && notes.previous.length > 0)
            return i18ndc("kdos-welcome", "@info:usagetip %1 is the running version, %2 the later one in the other slot", "KDOS went back to %1; %2, in the other slot, did not start or was not kept.", notes.version, notes.previous);
        if (notes !== null && typeof notes.previous === "string" && notes.previous.length > 0)
            return i18ndc("kdos-welcome", "@info:usagetip %1 is the previous version", "KDOS was updated from %1. If something you rely on misbehaves, System Settings → System Versions starts the previous version again.", notes.previous);
        if (page.live && typeof notes.version === "string")
            return i18ndc("kdos-welcome", "@info:usagetip %1 is the running version, on the live medium", "KDOS %1, running from the live medium.", notes.version);
        if (notes !== null && typeof notes.version === "string")
            return i18ndc("kdos-welcome", "@info:usagetip %1 is the running version", "KDOS %1. System Settings → System Versions shows the versions on this computer.", notes.version);
        return i18ndc("kdos-welcome", "@info:usagetip", "System Settings → System Versions shows the versions on this computer.");
    }

    Kdos.Run {
        id: run
    }

    property var notes: null
    property bool failed: false
    readonly property bool live: notes !== null && notes.live === true

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
            visible: !page.live
            text: i18ndc("kdos-welcome", "@action:button", "System Versions…")
            icon.name: "preferences-system"
            onClicked: run.launch(["kcmshell6", "kcm_kdos_system"], (code, text) => {})
            Accessible.name: text
        }
    }
}

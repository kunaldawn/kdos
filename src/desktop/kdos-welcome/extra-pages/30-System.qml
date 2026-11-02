/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   30-System.qml — two system slots, updates, and the previous version
 *
 * Reads `kdos-bootctl status --json`: which slot is running, whether a new
 * one is on trial, and whether the machine has two slots at all (a live
 * medium or a single-root install has one, and says so). Nothing on this
 * page changes a slot; the System Versions page in System Settings does,
 * and Discover installs updates. The page only explains them and opens them.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import "kdos" as Kdos

Welcome.ScrollablePage {
    id: page

    heading: i18ndc("kdos-welcome", "@title:window", "Your System")
    description: i18ndc("kdos-welcome", "@info:usagetip", "KDOS keeps two copies of the system, and only ever updates the one you are not running. If an update misbehaves, the previous version is one restart away.")

    Kdos.Run {
        id: run
    }

    property var slots: null
    property string failure: ""

    readonly property bool twoSlots: slots !== null && slots.configured === true
    // `running` is the slot this boot came from; `active` is the one the boot
    // menu starts by default, which differs while a new slot is on trial.
    readonly property string running: !twoSlots ? ""
        : (typeof slots.running === "string" ? slots.running.toUpperCase()
           : (typeof slots.active === "string" ? slots.active.toUpperCase() : ""))
    readonly property string other: running === "A" ? "B" : (running === "B" ? "A" : "")
    readonly property string trying: twoSlots && typeof slots.trying === "string" ? slots.trying.toUpperCase() : ""

    function open(argv: list<string>): void {
        run.run(argv, (code, text) => page.failure = code === 0 ? "" : text);
    }

    // A machine with one root makes kdos-bootctl exit 1, and Welcome Center
    // then hands back its own failure sentence instead of the JSON. Anything
    // that does not parse is therefore read as one root, which is what every
    // reply but a configured state means.
    Component.onCompleted: run.run(["kdos-bootctl", "status", "--json"], (code, text) => {
        try {
            page.slots = JSON.parse(text);
        } catch (e) {
            page.slots = { configured: false };
        }
    })

    view: Kdos.Flick {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.failure.length > 0
            text: page.failure
            showCloseButton: true
            onVisibleChanged: if (!visible) page.failure = ""
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: page.trying.length > 0
            text: i18ndc("kdos-welcome", "@info %1 is a slot letter", "Slot %1 is on trial: it becomes the one KDOS starts once this session has run cleanly.", page.trying)
        }

        Kdos.Point {
            icon: "drive-harddisk"
            title: page.twoSlots
                ? i18ndc("kdos-welcome", "@title %1 and %2 are slot letters", "Running slot %1; slot %2 holds the other version", page.running, page.other)
                : i18ndc("kdos-welcome", "@title", "One system copy on this machine")
            text: page.twoSlots
                ? i18ndc("kdos-welcome", "@info", "An update is written into the slot you are not running, checked against its signature, and used from the next restart. Your files, settings and boxes are kept apart from both slots and are the same in either.")
                : i18ndc("kdos-welcome", "@info", "This machine boots from a single system copy, as the live medium does, so there is no previous version to go back to. An installation onto a disk keeps two.")
        }

        Kdos.Point {
            icon: "system-software-update"
            title: i18ndc("kdos-welcome", "@title", "Updates")
            text: i18ndc("kdos-welcome", "@info", "Discover shows system updates beside packs and boxes. Nothing is downloaded on its own: KDOS looks for updates when you ask it to, and the tray tells you when one is ready to restart into.")
            action: i18ndc("kdos-welcome", "@action:button", "Check for Updates")
            actionIcon: "plasmadiscover"
            onTriggered: page.open(["plasma-discover", "--mode", "update"])
        }

        Kdos.Point {
            icon: "edit-undo-history"
            title: i18ndc("kdos-welcome", "@title", "Use the previous version")
            text: i18ndc("kdos-welcome", "@info", "System Settings → System Versions shows both slots and starts the other one for one boot, or for good. The boot menu offers it too, before anything else has started.")
            action: i18ndc("kdos-welcome", "@action:button", "System Versions…")
            actionIcon: "preferences-system"
            actionEnabled: page.twoSlots
            onTriggered: page.open(["kcmshell6", "kcm_kdos_system"])
        }
    }
}

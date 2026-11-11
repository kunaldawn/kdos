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
 * one is on trial, and how many slots the machine has. Three cases, each
 * with its own text:
 *
 *   two slots   configured true, slot_a and slot_b both name a filesystem
 *   one slot    configured true, slot_a or slot_b empty: an install too
 *               small for two, the reuse plan, or slots unticked; updates
 *               are applied over the running system with --in-place
 *   no state    configured false (kdos-bootctl exits 1), or a reply that
 *               does not parse: the live medium, which has no boot state
 *
 * Nothing on this page changes a slot; the System Versions page in System
 * Settings does, and Discover installs updates. The page only explains them
 * and opens them.
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
    description: page.twoSlots || page.slots === null
        ? i18ndc("kdos-welcome", "@info:usagetip", "KDOS keeps two copies of the system, and only ever updates the one you are not running. If an update misbehaves, the previous version is one restart away.")
        : (page.oneSlot
           ? i18ndc("kdos-welcome", "@info:usagetip", "An installation of KDOS on a disk with room for two keeps two copies of the system, and only ever updates the one you are not running. This one has a single copy.")
           : i18ndc("kdos-welcome", "@info:usagetip", "An installation of KDOS on a disk keeps two copies of the system, and only ever updates the one you are not running. The live medium has neither."))

    Kdos.Run {
        id: run
    }

    property var slots: null
    property string failure: ""

    readonly property bool configured: slots !== null && slots.configured === true
    readonly property bool twoSlots: configured
        && typeof slots.slot_a === "string" && slots.slot_a.length > 0
        && typeof slots.slot_b === "string" && slots.slot_b.length > 0
    readonly property bool oneSlot: configured && !twoSlots
    // `running` is the slot this boot came from; `active` is the one the boot
    // menu starts by default, which differs while a new slot is on trial.
    readonly property string running: !twoSlots ? ""
        : (typeof slots.running === "string" ? slots.running.toUpperCase()
           : (typeof slots.active === "string" ? slots.active.toUpperCase() : ""))
    readonly property string other: running === "A" ? "B" : (running === "B" ? "A" : "")
    readonly property string trying: twoSlots && typeof slots.trying === "string" ? slots.trying.toUpperCase() : ""

    function open(argv: list<string>): void {
        run.launch(argv, (code, text) => page.failure = text);
    }

    // With no boot state kdos-bootctl exits 1, and Welcome Center then hands
    // back its own failure sentence instead of the JSON; anything that does
    // not parse is therefore read as no boot state.
    Component.onCompleted: run.readJson(["kdos-bootctl", "status", "--json"], (code, value) => {
        page.slots = value !== null && typeof value === "object" ? value : { configured: false };
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
                : (page.oneSlot
                   ? i18ndc("kdos-welcome", "@title", "One system copy on this machine")
                   : i18ndc("kdos-welcome", "@title", "Running from the live medium"))
            text: page.twoSlots
                ? i18ndc("kdos-welcome", "@info", "An update is written into the slot you are not running, checked against its signature, and used from the next restart. Your files, settings and boxes are kept apart from both slots and are the same in either.")
                : (page.oneSlot
                   ? i18ndc("kdos-welcome", "@info", "This installation has one system slot, so an update can only be written over the running system, with <b>kdos update apply --in-place</b> in a terminal, and there is no previous version to go back to. A second slot can be added later; the KDOS book's Installation chapter says how.")
                   : i18ndc("kdos-welcome", "@info", "This session runs from the live medium, which has no system slots to update and no previous version to go back to. An installation onto a disk keeps two."))
        }

        Kdos.Point {
            icon: "system-software-update"
            title: i18ndc("kdos-welcome", "@title", "Updates")
            text: page.oneSlot
                ? i18ndc("kdos-welcome", "@info", "Discover shows updates to packs and boxes. It cannot install a system update on a machine with one slot; that is done in a terminal, as above. Nothing is downloaded on its own: KDOS looks for updates when you ask it to.")
                : i18ndc("kdos-welcome", "@info", "Discover shows system updates beside packs and boxes. Nothing is downloaded on its own: KDOS looks for updates when you ask it to, and the tray tells you when one is ready to restart into.")
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
            visible: page.twoSlots
            onTriggered: page.open(["kcmshell6", "kcm_kdos_system"])
        }
    }
}

/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   35-Persist.qml — keep the live session's changes on a stick
 *
 * On the live medium alone: a live session writes into RAM, and `kdos
 * persist create` gives it a store from the next boot. Live is Welcome
 * Center's Live mode, which its kded module starts with --live-environment
 * whenever plasma-welcomerc says LiveEnvironment=true. The page reads that
 * argument, because `show` is read the moment the page is made and must not
 * wait on a command; the mode itself lives in Welcome Center's private
 * module, which is compiled into the program, installs no module files to
 * check a page against, and owes a page no stability.
 *
 * `kdos persist --json` says what there is: the store's state (none,
 * present, refused, in-use) and device, the boot medium, and the disks a
 * store could be made on (`candidates`: the medium when it is writable, then
 * every removable writable disk with nothing mounted). A disc boot has a
 * read-only medium, so the page offers only the other candidates, and with
 * none it asks for a stick to be plugged in and looks again.
 *
 * The store is made as root through pkexec. The live medium's polkit rule
 * (49-kdos-installer-live.rules) grants `kdos persist create --yes` and
 * `kdos persist create /dev/<name> --yes` at the machine without a
 * password, because the live account's is published; anywhere else pkexec
 * asks for an administrator. The command for the medium names no device, so
 * the store lands on the disk `kdos persist` itself takes as the medium.
 *
 * It follows Your System, which on the live medium says the machine runs
 * from one system copy; installing is Welcome Center's own Live page, the
 * first of the tour, which launches plasma-welcomerc's LiveInstaller.
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

    show: Qt.application.arguments.indexOf("--live-environment") >= 0

    heading: i18ndc("kdos-welcome", "@title:window", "Keep Your Changes")
    description: page.storeState === "in-use"
        ? i18ndc("kdos-welcome", "@info:usagetip %1 is a device such as /dev/sda4", "This session is kept on %1: what you change is still there after the machine powers off.", page.storeDevice)
        : i18ndc("kdos-welcome", "@info:usagetip", "This session runs from memory: what you change is gone when the machine powers off. KDOS can keep it in the free space on a USB stick instead.")

    Kdos.Run {
        id: run
    }

    property var store: null
    property string failure: ""
    property bool created: false
    property int pick: 0

    readonly property string storeState: page.store !== null && typeof page.store.store === "string" ? page.store.store : ""
    readonly property string storeDevice: page.store !== null && typeof page.store.device === "string" ? page.store.device : ""
    readonly property string storeType: page.store !== null && typeof page.store.type === "string" ? page.store.type : ""
    readonly property string medium: page.store !== null && typeof page.store.medium === "string" ? page.store.medium : ""
    readonly property var candidates: page.store !== null && Array.isArray(page.store.candidates) ? page.store.candidates : []
    readonly property bool canCreate: page.storeState === "none" && !page.created
    readonly property string chosen: page.pick >= 0 && page.pick < page.candidates.length ? page.candidates[page.pick] : ""

    function refresh(): void {
        run.json(["kdos", "persist", "--json"], (code, value) => {
            page.pick = 0;
            page.store = value;
        });
    }

    function label(dev: string): string {
        return dev === page.medium
            ? i18ndc("kdos-welcome", "@item:inlistbox %1 is a device such as /dev/sda", "%1 (the stick you started from)", dev)
            : dev;
    }

    // The medium is created with no device named, the command line the
    // polkit rule grants for it; any other disk is named.
    function create(): void {
        const argv = ["pkexec", "/usr/local/bin/kdos", "persist", "create"];
        if (page.chosen !== page.medium) {
            argv.push(page.chosen);
        }
        argv.push("--yes");
        run.run(argv, (code, text) => {
            page.failure = code === 0 ? "" : text;
            page.created = code === 0;
            page.refresh();
        });
    }

    Component.onCompleted: refresh()

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
            type: Kirigami.MessageType.Positive
            visible: page.storeState === "present"
            text: i18ndc("kdos-welcome", "@info %1 is a device such as /dev/sda4", "A store is on %1. It is used from the next boot; choose “KDOS Live (clean session)” in the boot menu to start once without it.", page.storeDevice)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.storeState === "refused"
            text: i18ndc("kdos-welcome", "@info %1 is a device, %2 a filesystem type", "%1 is labelled KDOS_PERSIST but holds %2, which cannot keep a session's changes, so every boot ignores it. Reformat it as ext4, or take the label off it.", page.storeDevice, page.storeType)
        }

        Kdos.Point {
            icon: "drive-removable-media"
            title: i18ndc("kdos-welcome", "@title", "Persistence")
            text: page.storeState === "in-use" || page.storeState === "present"
                ? i18ndc("kdos-welcome", "@info %1 is a device such as /dev/sda4", "The store is %1, the partition labelled KDOS_PERSIST. To empty it, start “KDOS Live (clean session)” from the boot menu and run sudo kdos persist reset there.", page.storeDevice)
                : page.candidates.length > 0 || !page.canCreate
                ? i18ndc("kdos-welcome", "@info", "One new partition is made in the free space after the last one, labelled KDOS_PERSIST. The partitions already there are not changed, and a blank stick is given a partition table first. On the live medium no password is asked.")
                : i18ndc("kdos-welcome", "@info", "A disc cannot hold changes. Plug in a USB stick and look again: the store can go on any stick, and is found there at every boot.")
            // A store that exists already leaves nothing to do here, so the
            // button goes rather than greying.
            action: !page.canCreate
                ? ""
                : page.candidates.length === 0
                ? i18ndc("kdos-welcome", "@action:button", "Look Again")
                : page.chosen === page.medium
                    ? i18ndc("kdos-welcome", "@action:button", "Keep My Changes on This Stick")
                    : i18ndc("kdos-welcome", "@action:button %1 is a device such as /dev/sdb", "Keep My Changes on %1", page.chosen)
            actionIcon: page.candidates.length === 0 ? "view-refresh" : "document-save"
            actionEnabled: !run.busy && page.canCreate
            onTriggered: page.candidates.length === 0 ? page.refresh() : page.create()
        }

        RowLayout {
            Layout.fillWidth: true
            visible: page.canCreate && page.candidates.length > 1
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: i18ndc("kdos-welcome", "@label:listbox", "Keep them on:")
            }
            QQC2.ComboBox {
                Layout.fillWidth: true
                Accessible.name: i18ndc("kdos-welcome", "@label:listbox", "The disk to keep the changes on")
                enabled: !run.busy
                model: page.candidates.map(dev => page.label(dev))
                currentIndex: page.pick
                onActivated: index => page.pick = index
            }
        }
    }
}

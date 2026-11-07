/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   35-Persist.qml — keep the live session's changes on the stick
 *
 * On the live medium alone: a live session writes into RAM, and `kdos
 * persist create` gives it a store on the boot medium from the next boot.
 * Live is Welcome Center's Live mode, which its kded module starts with
 * --live-environment whenever plasma-welcomerc says LiveEnvironment=true.
 * The page reads that argument, because `show` is read the moment the page
 * is made and must not wait on a command; the mode itself lives in Welcome
 * Center's private module, which is compiled into the program, installs no
 * module files to check a page against, and owes a page no stability.
 * The store is made as root through pkexec. The live medium's polkit rule
 * (49-kdos-installer-live.rules) grants that one command line at the
 * machine without a password, because the live account's is published;
 * anywhere else pkexec asks for an administrator. `kdos persist --json`
 * says whether a store already exists or is in use, and the button is
 * offered only when there is none.
 *
 * It follows Your System, which on the live medium says the machine runs
 * from one system copy; installing is Welcome Center's own Live page, the
 * first of the tour, which launches plasma-welcomerc's LiveInstaller.
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

    show: Qt.application.arguments.indexOf("--live-environment") >= 0

    heading: i18ndc("kdos-welcome", "@title:window", "Keep Your Changes")
    description: i18ndc("kdos-welcome", "@info:usagetip", "This session runs from memory: what you change is gone when the machine powers off. KDOS can keep it in the free space on this stick instead.")

    Kdos.Run {
        id: run
    }

    property var store: null
    property string failure: ""
    property bool created: false

    readonly property string storeState: store !== null && typeof store.store === "string" ? store.store : ""

    function refresh(): void {
        run.json(["kdos", "persist", "--json"], (code, value) => page.store = value);
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
            visible: page.created || page.storeState === "present"
            text: i18ndc("kdos-welcome", "@info", "A store is on this stick. It is used from the next boot; choose “KDOS Live (clean session)” in the boot menu to start once without it.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: page.storeState === "in-use"
            text: i18ndc("kdos-welcome", "@info", "This session is already being kept on the stick.")
        }

        Kdos.Point {
            icon: "media-removable"
            title: i18ndc("kdos-welcome", "@title", "Persistence")
            text: i18ndc("kdos-welcome", "@info", "One new partition is made after the image, labelled KDOS_PERSIST. The partitions already on the stick are not changed. You will be asked for permission.")
            action: i18ndc("kdos-welcome", "@action:button", "Keep My Changes on This Stick")
            actionIcon: "document-save"
            actionEnabled: !run.busy && !page.created && (page.storeState === "none" || page.storeState === "")
            // The absolute path is the command line the live medium's polkit
            // rule grants without a password (49-kdos-installer-live.rules).
            onTriggered: run.run(["pkexec", "/usr/local/bin/kdos", "persist", "create", "--yes"], (code, text) => {
                page.failure = code === 0 ? "" : text;
                page.created = code === 0;
                page.refresh();
            })
        }
    }
}

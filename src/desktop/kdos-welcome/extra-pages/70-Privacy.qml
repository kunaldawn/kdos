/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   70-Privacy.qml — what reaches the network, and the wallet
 *
 * Three facts a person should not have to discover: KDOS downloads nothing
 * on its own, and Firefox's background traffic of its own is its security
 * data (Safe Browsing and tracking-protection lists, remote settings'
 * certificate revocations and blocklists, add-on update checks) and its
 * push connection (the firefox-esr port's kdos.js and policies turn off
 * its studies, telemetry and connectivity probes and leave exactly these
 * on, so the sentence must change with them); the firewall lets services in by
 * name, and anything that listens (KDE Connect pairing a phone, desktop
 * sharing) needs its name allowed; and with automatic login there is no
 * password at login for the wallet to open with, so it asks for its own on
 * first use. The last is shown only on a machine that logs in
 * automatically, read from SDDM's kde_settings.conf. On the live medium,
 * which always logs in automatically, the wallet is off
 * (/usr/share/kdos/xdg-live/kwalletrc), so the paragraph says the live
 * session keeps no wallet and an installed system's asks for its own
 * password, and offers no Wallet Settings button, which would open on a
 * switched-off subsystem. Live is the --live-environment argument Welcome
 * Center's kded module and the live medium's launcher entry pass, the one
 * 35-Persist.qml reads.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Privacy and Network")
    description: i18ndc("kdos-welcome", "@info:usagetip", "KDOS itself phones home for nothing: updates, firmware and add-ons are fetched only when you ask, and nothing listens on the network until you allow it. Firefox keeps its own security data current from Mozilla and Google: its Safe Browsing and tracking-protection lists, certificate revocations and blocklists, and updates for its add-ons. It also keeps a connection open to Mozilla for site notifications. None of these sends the pages you visit.")

    Kdos.Run {
        id: run
    }

    readonly property bool live: Qt.application.arguments.indexOf("--live-environment") >= 0
    property string autologinUser: ""
    property string failure: ""

    function open(argv: list<string>): void {
        run.launch(argv, (code, text) => page.failure = text);
    }

    Component.onCompleted: run.config("/etc/sddm.conf.d/kde_settings.conf", "Autologin", "User", "", user => page.autologinUser = user)

    view: Kdos.Flick {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.failure.length > 0
            text: page.failure
            showCloseButton: true
            onVisibleChanged: if (!visible) page.failure = ""
        }

        Kdos.Point {
            icon: "security-high"
            title: i18ndc("kdos-welcome", "@title", "The firewall allows services by name")
            text: i18ndc("kdos-welcome", "@info", "Incoming connections are closed. To pair a phone with KDE Connect, share this screen, or serve the library to other machines, allow that service by name; KDOS asks the first time something tries.")
            action: i18ndc("kdos-welcome", "@action:button", "Firewall…")
            actionIcon: "security-high"
            onTriggered: page.open(["kcmshell6", "kcm_kdos_firewall"])
        }

        Kdos.Point {
            icon: "preferences-system-session-services"
            title: i18ndc("kdos-welcome", "@title", "Background services")
            text: i18ndc("kdos-welcome", "@info", "Services the desktop runs for you, such as clipboard history and device notifications, can be stopped and kept from starting.")
            action: i18ndc("kdos-welcome", "@action:button", "Background Services…")
            actionIcon: "preferences-system-session-services"
            onTriggered: page.open(["kcmshell6", "kcm_kded"])
        }

        Kdos.Point {
            visible: page.live || page.autologinUser.length > 0
            icon: "kwalletmanager"
            title: i18ndc("kdos-welcome", "@title", "The wallet and automatic login")
            text: page.live
                ? i18ndc("kdos-welcome", "@info", "The live session logs in without a password and keeps no wallet: nothing it is asked to save is kept, and a program that asks to save a password finds no wallet to save it in. On an installed system that logs in automatically, the wallet asks for its own password the first time an application needs it.")
                : i18ndc("kdos-welcome", "@info", "This machine logs in without a password, so the wallet that keeps your saved passwords cannot be opened by your login. It asks for its own password the first time an application needs it; you may leave that empty if the disk is encrypted.")
            action: page.live ? "" : i18ndc("kdos-welcome", "@action:button", "Wallet Settings…")
            actionIcon: "kwalletmanager"
            onTriggered: page.open(["kcmshell6", "kcm_kwallet5"])
        }
    }
}

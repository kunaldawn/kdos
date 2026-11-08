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
 * on its own; the firewall lets services in by name, and anything that
 * listens (KDE Connect pairing a phone, desktop sharing) needs its name
 * allowed; and with automatic login there is no password at login for the
 * wallet to open with, so it asks for its own on first use. The last is
 * shown only on a machine that logs in automatically, read from SDDM's
 * kde_settings.conf.
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
    description: i18ndc("kdos-welcome", "@info:usagetip", "Nothing on KDOS phones home. Updates, firmware and add-ons are fetched only when you ask, and nothing listens on the network until you allow it.")

    Kdos.Run {
        id: run
    }

    property string autologinUser: ""
    property string failure: ""

    function open(argv: list<string>): void {
        run.run(argv, (code, text) => page.failure = code === 0 ? "" : text);
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
            visible: page.autologinUser.length > 0
            icon: "kwalletmanager"
            title: i18ndc("kdos-welcome", "@title", "The wallet and automatic login")
            text: i18ndc("kdos-welcome", "@info", "This machine logs in without a password, so the wallet that keeps your saved passwords cannot be opened by your login. It asks for its own password the first time an application needs it; you may leave that empty if the disk is encrypted.")
            action: i18ndc("kdos-welcome", "@action:button", "Wallet Settings…")
            actionIcon: "kwalletmanager"
            onTriggered: page.open(["kcmshell6", "kcm_kwallet5"])
        }
    }
}

/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   80-Help.qml — where answers are, on this machine
 *
 * The KDOS book and KDOS from Scratch are HTML under /usr/share/doc, opened
 * in the browser. KDOS from Scratch is there only once kdos-kfs is
 * installed, which the page tells by that port's desktop entry,
 * kdos-library-kfs-book: without it the point says how to install it and its
 * button is greyed rather than opening a missing file.
 * `kdos help` and the manual pages are the terminal's way in. Nothing here
 * needs the network.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Help")
    description: i18ndc("kdos-welcome", "@info:usagetip", "Every part of KDOS is written down, and the KDOS book is on this machine.")

    Kdos.Run {
        id: run
    }

    property string failure: ""

    readonly property Welcome.ApplicationInfo kfs: Welcome.ApplicationInfo {
        desktopName: "kdos-library-kfs-book"
    }

    function open(argv: list<string>): void {
        run.launch(argv, (code, text) => page.failure = text);
    }

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
            icon: "help-contents"
            title: i18ndc("kdos-welcome", "@title", "The KDOS book")
            text: i18ndc("kdos-welcome", "@info", "How KDOS works and why, from the first boot to every command. The Help button on a KDOS settings page opens its chapter.")
            action: i18ndc("kdos-welcome", "@action:button", "Open the Book")
            actionIcon: "help-contents"
            onTriggered: page.open(["xdg-open", "file:///usr/share/doc/kdos/index.html"])
        }

        Kdos.Point {
            icon: "run-build"
            title: i18ndc("kdos-welcome", "@title", "KDOS from Scratch")
            text: page.kfs.exists
                ? i18ndc("kdos-welcome", "@info", "The second book: how this image was built, phase by phase and port by port.")
                : i18ndc("kdos-welcome", "@info", "The second book: how this image was built, phase by phase and port by port. It is not on this machine; install it with <b>sudo kdos update install kdos-kfs</b>.")
            action: i18ndc("kdos-welcome", "@action:button", "Open KDOS from Scratch")
            actionIcon: "run-build"
            actionEnabled: page.kfs.exists
            onTriggered: page.open(["xdg-open", "file:///usr/share/doc/kfs/index.html"])
        }

        Kdos.Point {
            icon: "utilities-terminal"
            title: i18ndc("kdos-welcome", "@title", "In a terminal")
            text: i18ndc("kdos-welcome", "@info", "<b>kdos help</b> lists every command, <b>man kdos</b> is the manual, and <b>kdos doctor</b> checks this machine and says what is wrong.")
            action: i18ndc("kdos-welcome", "@action:button", "Run kdos doctor")
            actionIcon: "utilities-terminal"
            onTriggered: page.open(["konsole", "--hold", "-e", "kdos", "doctor"])
        }

        Kdos.Point {
            icon: "start-here-kdos"
            title: i18ndc("kdos-welcome", "@title", "This tour")
            text: i18ndc("kdos-welcome", "@info", "Welcome Center can be opened again at any time from the application launcher.")
        }
    }
}

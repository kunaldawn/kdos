/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   40-Software.qml — where software comes from, and what a box is
 *
 * KDOS software has three shapes, all in Discover: ports built for the
 * system, packs mounted beside it, and boxes, applications from other
 * distributions run in a container. The page explains the shapes and the
 * three confinement states a boxed window's badge reports (confined,
 * tagged, not confined), and opens Discover and the Boxes settings page. It
 * changes nothing itself.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Software and Boxes")
    description: i18ndc("kdos-welcome", "@info:usagetip", "Everything you can install is in Discover, and all of it comes from KDOS: there is no store on the internet to sign in to.")

    Kdos.Run {
        id: run
    }

    property string failure: ""

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
            icon: "plasmadiscover"
            title: i18ndc("kdos-welcome", "@title", "Discover")
            text: i18ndc("kdos-welcome", "@info", "<b>Ports</b> are built into the system. <b>Packs</b> are larger sets, such as a whole desktop of tools, that mount beside it and can be removed in one step. <b>Boxes</b> run applications made for other distributions, each in its own container.")
            action: i18ndc("kdos-welcome", "@action:button", "Open Discover")
            actionIcon: "plasmadiscover"
            onTriggered: Welcome.Utils.launchApp("org.kde.discover")
        }

        Kdos.Point {
            icon: "security-high"
            title: i18ndc("kdos-welcome", "@title", "What a box may touch")
            text: i18ndc("kdos-welcome", "@info", "A boxed window carries a badge in its title bar and in the task manager, and its tooltip says which boundary held. <b>Confined</b>: the box has its own runtime folder and only a filtered view of the session bus. <b>Tagged</b>: the desktop keeps it away from other windows, but it could still reach the session directly. <b>Not confined</b>: the box fell back to the desktop's own socket; the badge becomes a warning and the title bar loses its box colour.")
        }

        Kdos.Point {
            icon: "preferences-system"
            title: i18ndc("kdos-welcome", "@title", "Boxes in System Settings")
            text: i18ndc("kdos-welcome", "@info", "Each box can be given or refused your files, the microphone and camera, and the network, and started, stopped or thrown away.")
            action: i18ndc("kdos-welcome", "@action:button", "Boxes…")
            actionIcon: "preferences-system"
            onTriggered: page.open(["kcmshell6", "kcm_kdos_boxes"])
        }
    }
}

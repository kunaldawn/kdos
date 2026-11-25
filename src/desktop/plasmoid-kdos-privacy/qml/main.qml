/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   main.qml — the privacy lamps
 *
 * Three lamps, microphone, camera and screen, each shown only while
 * something captures it. Clicking a lamp mutes the microphone captures, or
 * stops the camera's or the screen's; the middle button, or Show What Is
 * Capturing in the menu, opens the list with a button per capture. Hidden
 * from the tray while nothing captures, so it never takes a slot for nothing.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import plasma.applet.org.kdos.privacy as Privacy

PlasmoidItem {
    id: root

    readonly property Privacy.PrivacyApplet privacy: Plasmoid as Privacy.PrivacyApplet

    readonly property bool anyActive: root.privacy.microphoneActive || root.privacy.cameraActive || root.privacy.screenActive

    Plasmoid.icon: root.privacy.screenActive ? "media-record-symbolic"
                 : root.privacy.cameraActive ? "camera-on-symbolic"
                 : root.privacy.microphoneActive ? (root.privacy.microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic")
                 : "security-high-symbolic"
    Plasmoid.status: root.anyActive ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    Plasmoid.title: i18ndc("kdos-plasmoid-privacy", "@title", "Privacy Lamps")

    /* No tooltip while the list is open: the lamp's own MouseArea takes the
     * press the tray would hide the tooltip on, and a tooltip with no text
     * is hidden, so it does not sit over the list. */
    toolTipMainText: root.expanded ? ""
                   : root.anyActive ? i18ndc("kdos-plasmoid-privacy", "@info:tooltip", "Something is capturing")
                   : i18ndc("kdos-plasmoid-privacy", "@info:tooltip", "Nothing is capturing")
    toolTipSubText: {
        if (root.expanded) {
            return "";
        }
        if (!root.privacy.available && root.privacy.problem.length > 0) {
            return root.privacy.problem;
        }
        const lines = [];
        for (const kind of ["mic", "camera", "screen"]) {
            const s = root.privacy.summaries[kind] ?? "";
            if (s.length > 0) {
                lines.push(s);
            }
        }
        return lines.join("\n");
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18ndc("kdos-plasmoid-privacy", "@action:inmenu", "Show What Is Capturing")
            icon.name: "view-list-details-symbolic"
            onTriggered: root.expanded = true
        }
    ]

    compactRepresentation: RowLayout {
        id: lamps

        spacing: 0
        Layout.minimumWidth: implicitWidth
        Layout.minimumHeight: Kirigami.Units.iconSizes.small

        Lamp {
            kind: "mic"
            visible: root.privacy.microphoneActive || !root.anyActive
            lit: root.privacy.microphoneActive
            source: root.privacy.microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic"
            actionText: root.privacy.microphoneMuted ? i18ndc("kdos-plasmoid-privacy", "@action:button", "Unmute the microphone")
                                                 : i18ndc("kdos-plasmoid-privacy", "@action:button", "Mute the microphone")
            onToggleList: root.expanded = !root.expanded
        }
        Lamp {
            kind: "camera"
            visible: root.privacy.cameraActive
            lit: true
            source: "camera-on-symbolic"
            actionText: i18ndc("kdos-plasmoid-privacy", "@action:button", "Stop using the camera")
            onToggleList: root.expanded = !root.expanded
        }
        Lamp {
            kind: "screen"
            visible: root.privacy.screenActive
            lit: true
            source: "media-record-symbolic"
            actionText: i18ndc("kdos-plasmoid-privacy", "@action:button", "Stop sharing the screen")
            onToggleList: root.expanded = !root.expanded
        }
    }

    fullRepresentation: FullRepresentation {}
}

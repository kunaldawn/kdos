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

PlasmoidItem {
    id: root

    readonly property bool anyActive: Plasmoid.microphoneActive || Plasmoid.cameraActive || Plasmoid.screenActive

    Plasmoid.icon: Plasmoid.screenActive ? "media-record-symbolic"
                 : Plasmoid.cameraActive ? "camera-on-symbolic"
                 : Plasmoid.microphoneActive ? (Plasmoid.microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic")
                 : "security-high-symbolic"
    Plasmoid.status: root.anyActive ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    Plasmoid.title: i18ndc("kdos-plasmoid-privacy", "@title", "Privacy Lamps")

    toolTipMainText: root.anyActive ? i18ndc("kdos-plasmoid-privacy", "@info:tooltip", "Something is capturing")
                                    : i18ndc("kdos-plasmoid-privacy", "@info:tooltip", "Nothing is capturing")
    toolTipSubText: {
        if (!Plasmoid.available && Plasmoid.problem.length > 0) {
            return Plasmoid.problem;
        }
        const lines = [];
        for (const kind of ["mic", "camera", "screen"]) {
            const s = Plasmoid.summaries[kind] ?? "";
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
            visible: Plasmoid.microphoneActive || !root.anyActive
            lit: Plasmoid.microphoneActive
            source: Plasmoid.microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic"
            actionText: Plasmoid.microphoneMuted ? i18ndc("kdos-plasmoid-privacy", "@action:button", "Unmute the microphone")
                                                 : i18ndc("kdos-plasmoid-privacy", "@action:button", "Mute the microphone")
            onToggleList: root.expanded = !root.expanded
        }
        Lamp {
            kind: "camera"
            visible: Plasmoid.cameraActive
            lit: true
            source: "camera-on-symbolic"
            actionText: i18ndc("kdos-plasmoid-privacy", "@action:button", "Stop using the camera")
            onToggleList: root.expanded = !root.expanded
        }
        Lamp {
            kind: "screen"
            visible: Plasmoid.screenActive
            lit: true
            source: "media-record-symbolic"
            actionText: i18ndc("kdos-plasmoid-privacy", "@action:button", "Stop sharing the screen")
            onToggleList: root.expanded = !root.expanded
        }
    }

    fullRepresentation: FullRepresentation {}
}

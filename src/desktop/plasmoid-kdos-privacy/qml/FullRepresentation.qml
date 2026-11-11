/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   FullRepresentation.qml — what is capturing, one row each
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

PlasmaExtras.Representation {
    id: full

    Layout.minimumWidth: Kirigami.Units.gridUnit * 18
    Layout.minimumHeight: Kirigami.Units.gridUnit * 10
    Layout.preferredHeight: Kirigami.Units.gridUnit * 16
    collapseMarginsHint: true

    property string failure: ""

    Connections {
        target: Plasmoid
        function onActionFailed(message: string): void {
            full.failure = message;
        }
    }

    header: PlasmaExtras.PlasmoidHeading {
        visible: !(Plasmoid.containmentDisplayHints & PlasmaCore.Types.ContainmentDrawsPlasmoidHeading)
        contentItem: Kirigami.Heading {
            level: 1
            text: i18ndc("kdos-plasmoid-privacy", "@title", "Privacy Lamps")
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
    }

    contentItem: PlasmaComponents.ScrollView {
        ListView {
            id: list

            model: Plasmoid.captures
            focus: true
            clip: true
            spacing: Kirigami.Units.smallSpacing
            reuseItems: false
            Accessible.name: i18ndc("kdos-plasmoid-privacy", "@info:accessible", "Programs capturing")

            header: Kirigami.InlineMessage {
                width: list.width
                visible: full.failure.length > 0 || (!Plasmoid.available && Plasmoid.problem.length > 0)
                type: full.failure.length > 0 ? Kirigami.MessageType.Error : Kirigami.MessageType.Warning
                text: full.failure.length > 0 ? full.failure : Plasmoid.problem
                showCloseButton: full.failure.length > 0
                onVisibleChanged: if (!visible) full.failure = ""
            }

            section.property: "kind"
            section.delegate: Kirigami.ListSectionHeader {
                required property string section
                width: list.width
                text: section === "mic" ? i18ndc("kdos-plasmoid-privacy", "@title:group", "Microphone")
                    : section === "camera" ? i18ndc("kdos-plasmoid-privacy", "@title:group", "Camera")
                    : i18ndc("kdos-plasmoid-privacy", "@title:group", "Screen")
            }

            delegate: PlasmaComponents.ItemDelegate {
                id: row

                required property var modelData

                width: list.width
                hoverEnabled: false
                down: false
                Accessible.name: row.modelData.label
                Accessible.description: row.modelData.muted ? i18ndc("kdos-plasmoid-privacy", "@info:accessible", "Muted") : ""

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                        source: row.modelData.icon.length > 0 ? row.modelData.icon
                              : row.modelData.kind === "camera" ? "camera-on-symbolic"
                              : row.modelData.kind === "screen" ? "media-record-symbolic"
                              : "audio-input-microphone-symbolic"
                        Accessible.ignored: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: row.modelData.label
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            visible: row.modelData.muted
                            text: i18ndc("kdos-plasmoid-privacy", "@info", "Muted")
                            font: Kirigami.Theme.smallFont
                            opacity: 0.75
                        }
                    }

                    PlasmaComponents.ToolButton {
                        visible: row.modelData.canMute
                        icon.name: row.modelData.muted ? "audio-input-microphone-symbolic" : "microphone-sensitivity-muted-symbolic"
                        text: row.modelData.muted ? i18ndc("kdos-plasmoid-privacy", "@action:button", "Unmute")
                                                  : i18ndc("kdos-plasmoid-privacy", "@action:button", "Mute")
                        Accessible.name: row.modelData.muted ? i18ndc("kdos-plasmoid-privacy", "@action:button %1 a program", "Unmute %1", row.modelData.label)
                                                             : i18ndc("kdos-plasmoid-privacy", "@action:button %1 a program", "Mute %1", row.modelData.label)
                        onClicked: Plasmoid.setMuted(row.modelData.id, !row.modelData.muted)
                    }

                    PlasmaComponents.ToolButton {
                        visible: row.modelData.canStop
                        icon.name: "media-playback-stop-symbolic"
                        text: i18ndc("kdos-plasmoid-privacy", "@action:button", "Stop")
                        Accessible.name: i18ndc("kdos-plasmoid-privacy", "@action:button %1 a program", "Stop %1 capturing", row.modelData.label)
                        onClicked: Plasmoid.stop(row.modelData.id)
                    }
                }
            }

            PlasmaExtras.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: list.count === 0
                iconName: "security-high-symbolic"
                text: i18ndc("kdos-plasmoid-privacy", "@info", "Nothing is capturing")
                explanation: i18ndc("kdos-plasmoid-privacy", "@info", "A lamp lights here while a program records from a microphone or a camera, or shares the screen, and names its box.")
            }
        }
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   LookPage — the accent and the desk the first login starts with
 *
 * The accents are the ones kinstall accepts (`--dump look`), each shown by its
 * global theme's preview, or by a swatch painted from its own colour scheme
 * where no preview is installed. Both are changed later on the Global Theme
 * page, with `kdos theme`, or with `kdos desk`.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Look")
    readonly property var answers: Backend.answers
    readonly property var accents: Backend.look.accents || []

    onEntered: {
        if (accents.length === 0) {
            Backend.refreshLook();
        }
    }

    Kirigami.Heading {
        level: 3
        text: i18nc("@title:group", "Accent")
    }

    GridLayout {
        Layout.fillWidth: true
        columns: Math.max(2, Math.floor(page.availableWidth / (Kirigami.Units.gridUnit * 10)))
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.largeSpacing

        Repeater {
            model: page.accents
            delegate: QQC2.ItemDelegate {
                id: card
                required property var modelData
                readonly property bool chosen: modelData.name === page.answers.theme
                readonly property var colors: Backend.accentColors(modelData.name)
                readonly property string preview: Backend.accentPreview(modelData.lookandfeel)
                readonly property string label: colors.name
                    ? colors.name.replace(/^KDOS /, "")
                    : modelData.label.charAt(0) + modelData.label.slice(1).toLowerCase()

                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 7
                highlighted: chosen
                checkable: true
                checked: chosen
                Accessible.name: chosen ? i18nc("@info:whatsthis %1 accent", "%1 accent, chosen", label)
                                        : i18nc("@info:whatsthis %1 accent", "%1 accent", label)
                onClicked: page.answers.theme = modelData.name

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Image {
                            anchors.fill: parent
                            source: card.preview
                            fillMode: Image.PreserveAspectFit
                            visible: card.preview.length > 0
                            asynchronous: true
                        }
                        /* The scheme's own colours, where its preview is not
                         * installed: the window, a selection on it and its text. */
                        Rectangle {
                            anchors.fill: parent
                            visible: card.preview.length === 0
                            radius: Kirigami.Units.cornerRadius
                            color: card.colors.window || Kirigami.Theme.backgroundColor
                            border.color: Kirigami.Theme.disabledTextColor
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Kirigami.Units.largeSpacing
                                height: Kirigami.Units.gridUnit * 1.5
                                radius: Kirigami.Units.cornerRadius
                                color: card.colors.highlight || Kirigami.Theme.highlightColor
                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: card.label
                                    color: card.colors.highlightedText || Kirigami.Theme.highlightedTextColor
                                }
                            }
                        }
                    }
                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: card.modelData.default
                            ? i18nc("@item %1 accent", "%1 (default)", card.label) : card.label
                        font.bold: card.chosen
                    }
                }
            }
        }
    }

    Kirigami.Heading {
        Layout.topMargin: Kirigami.Units.largeSpacing
        level: 3
        text: i18nc("@title:group", "Desk")
    }

    Repeater {
        model: [
            { name: "kdos",   icon: "preferences-desktop-theme-global",
              title: i18nc("@option:radio desk", "KDOS"),
              text: i18n("One panel at the bottom: the menu, the tasks, the meters, the tray and the clock.") },
            { name: "dock",   icon: "preferences-desktop",
              title: i18nc("@option:radio desk", "Dock"),
              text: i18n("A bar along the top with the menu and the clock, and a floating dock below.") },
            { name: "tiling", icon: "view-grid",
              title: i18nc("@option:radio desk", "Tiling"),
              text: i18n("A bar along the top, and windows tiled side by side as they open.") }
        ]
        delegate: QQC2.RadioDelegate {
            id: desk
            required property var modelData
            Layout.fillWidth: true
            icon.name: modelData.icon
            text: modelData.title + " — " + modelData.text
            checked: page.answers.desk === modelData.name
            Accessible.name: modelData.title + ": " + modelData.text
            onToggled: page.answers.desk = modelData.name
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon {
                    source: desk.modelData.icon
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        text: desk.modelData.title
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: desk.modelData.text
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}

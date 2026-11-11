/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * The box's colour, as swatches of the title bars KWin will paint. Each
 * swatch is a miniature title bar in the installed scheme's own colours
 * (AccentModel): its background, a sample of its title text, and a stripe
 * of its accent, so what is picked here is what the window wears and two
 * near-black title bars are still told apart at a glance. A scheme that is
 * not installed cannot be picked: its swatch is drawn empty and says why.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Flow {
    id: picker

    required property string current

    signal picked(string accentId)

    readonly property real swatchSize: Kirigami.Units.gridUnit * 2.5

    Layout.fillWidth: true
    /* Wide enough for every swatch on one row; a narrower page wraps. */
    Layout.maximumWidth: swatchRepeater.count * (swatchSize + spacing)
    spacing: Kirigami.Units.smallSpacing

    QQC2.ButtonGroup {
        id: swatches
    }

    Repeater {
        id: swatchRepeater

        model: kcm.accents

        delegate: QQC2.AbstractButton {
            id: swatch

            QQC2.ButtonGroup.group: swatches

            required property string accentId
            required property string label
            required property color swatchColor
            required property color swatchTextColor
            required property color swatchAccentColor
            required property bool available

            implicitWidth: picker.swatchSize
            implicitHeight: picker.swatchSize
            /* Checkable and exclusive, so a click and a screen reader's
             * Toggle both land in onCheckedChanged; Press alone does not
             * reach a button whose role is a radio button. */
            checkable: true
            checked: picker.current === accentId
            /* Neutral is the profile's default and needs no scheme of its
             * own to be chosen: without KDOSBox installed it is drawn in
             * the page's colours. */
            enabled: available || accentId === "session"
            focusPolicy: Qt.StrongFocus

            Accessible.role: Accessible.RadioButton
            Accessible.name: label
            Accessible.description: available || accentId === "session" ? "" : i18nc("@info:whatsthis", "Its colour scheme is not installed.")

            QQC2.ToolTip.text: available || accentId === "session" ? label : i18nc("@info:tooltip %1 accent", "%1: its colour scheme is not installed", label)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            onCheckedChanged: {
                if (checked && picker.current !== accentId) {
                    picker.picked(accentId);
                }
            }

            contentItem: Rectangle {
                id: bar

                readonly property int frame: swatch.checked || swatch.visualFocus ? 3 : 1

                color: swatch.available ? swatch.swatchColor : Kirigami.Theme.backgroundColor
                border.width: frame
                border.color: swatch.checked || swatch.visualFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor

                /* The accent, where a title bar's focus line is. */
                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: bar.frame
                    }
                    height: Math.max(2, Math.round(Kirigami.Units.smallSpacing))
                    visible: swatch.available
                    color: swatch.swatchAccentColor
                }

                /* A sample of the title, in the title's colour; the chosen
                 * swatch shows a checkmark in its place. */
                QQC2.Label {
                    anchors.centerIn: parent
                    visible: swatch.available && !swatch.checked
                    text: i18nc("@label a sample of title-bar text; keep it two letters", "Aa")
                    color: swatch.swatchTextColor
                    font.bold: true
                    Accessible.ignored: true
                }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Kirigami.Units.iconSizes.small
                    height: width
                    visible: swatch.checked
                    source: "dialog-ok-apply-symbolic"
                    color: swatch.available ? swatch.swatchTextColor : Kirigami.Theme.textColor
                    isMask: true
                }
            }
        }
    }
}

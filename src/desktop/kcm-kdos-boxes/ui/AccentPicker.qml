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
 * swatch is the installed scheme's own colour (AccentModel), so what is
 * picked here is what the window wears. A scheme that is not installed
 * cannot be picked: its swatch is drawn empty and says why.
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

    Layout.fillWidth: true
    Layout.maximumWidth: Kirigami.Units.gridUnit * 24
    spacing: Kirigami.Units.smallSpacing

    QQC2.ButtonGroup {
        id: swatches
    }

    Repeater {
        model: kcm.accents

        delegate: QQC2.AbstractButton {
            id: swatch

            QQC2.ButtonGroup.group: swatches

            required property string accentId
            required property string label
            required property color swatchColor
            required property color swatchTextColor
            required property bool available

            implicitWidth: Kirigami.Units.gridUnit * 2.5
            implicitHeight: Kirigami.Units.gridUnit * 2.5
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
                color: swatch.available ? swatch.swatchColor : Kirigami.Theme.backgroundColor
                border.width: swatch.checked || swatch.visualFocus ? 3 : 1
                border.color: swatch.checked || swatch.visualFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Kirigami.Units.iconSizes.small
                    height: width
                    visible: swatch.checked
                    source: "checkmark"
                    color: swatch.available ? swatch.swatchTextColor : Kirigami.Theme.textColor
                    isMask: true
                }
            }
        }
    }
}

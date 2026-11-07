/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   AccentSwatch.qml — one accent, painted in its own scheme
 *
 * The plate is the accent's ground, the name its ink and the bar its
 * selection, all three read from the accent's colour scheme by Accents.qml.
 * The frame is drawn in the RUNNING scheme's roles, so the one in force and
 * the one with focus are marked in the colours around the swatch, not in
 * the swatch's own. Square, as every KDOS plate is.
 * ---------------------------------
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

QQC2.AbstractButton {
    id: swatch

    required property string name
    required property string title
    required property string ground
    required property string ink
    required property string selection
    required property bool light
    required property bool known
    property bool current: false

    implicitWidth: Kirigami.Units.gridUnit * 7
    implicitHeight: Kirigami.Units.gridUnit * 4
    padding: Kirigami.Units.smallSpacing
    // Clear of the accent bar along the bottom.
    bottomPadding: Kirigami.Units.smallSpacing * 4
    enabled: known
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.RadioButton
    Accessible.name: title
    Accessible.description: current
        ? i18ndc("kdos-welcome", "@info accessible description of the accent in use", "In use")
        : (light ? i18ndc("kdos-welcome", "@info accessible description", "Light accent")
                 : i18ndc("kdos-welcome", "@info accessible description", "Dark accent"))
    Accessible.checkable: true
    Accessible.checked: current

    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.text: i18ndc("kdos-welcome", "@info:tooltip %1 is an accent name", "Use the %1 accent everywhere", title)
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

    background: Rectangle {
        color: swatch.known ? swatch.ground : Kirigami.Theme.backgroundColor
        border.width: swatch.current ? 3 : ((swatch.visualFocus || swatch.hovered) ? 2 : 1)
        border.color: (swatch.current || swatch.visualFocus || swatch.hovered)
            ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: parent.border.width
            }
            height: Kirigami.Units.smallSpacing * 2
            color: swatch.known ? swatch.selection : Kirigami.Theme.disabledTextColor
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        QQC2.Label {
            Layout.alignment: Qt.AlignHCenter
            text: swatch.title.toUpperCase()
            color: swatch.known ? swatch.ink : Kirigami.Theme.disabledTextColor
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.bold: swatch.current
            textFormat: Text.PlainText
        }
        QQC2.Label {
            Layout.alignment: Qt.AlignHCenter
            text: swatch.light ? i18ndc("kdos-welcome", "@label an accent with a light ground", "light")
                               : i18ndc("kdos-welcome", "@label an accent with a dark ground", "dark")
            color: swatch.known ? swatch.ink : Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            textFormat: Text.PlainText
        }
    }
}

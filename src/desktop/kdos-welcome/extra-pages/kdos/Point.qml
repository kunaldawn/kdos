/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Point.qml — one thing a page explains: an icon, a title, a sentence
 *
 * The tour's explanatory pages are lists of these. An optional button runs
 * the page's action for the point; the page decides what that is.
 * ---------------------------------
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

RowLayout {
    id: point

    property string icon
    property string title
    property string text
    property string action
    property string actionIcon
    property bool actionEnabled: true

    signal triggered()

    Layout.fillWidth: true
    spacing: Kirigami.Units.largeSpacing

    Accessible.role: Accessible.Grouping
    Accessible.name: title

    Kirigami.Icon {
        Layout.alignment: Qt.AlignTop
        implicitWidth: Kirigami.Units.iconSizes.large
        implicitHeight: Kirigami.Units.iconSizes.large
        source: point.icon
        Accessible.ignored: true
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 3
            text: point.title
            wrapMode: Text.WordWrap
        }
        QQC2.Label {
            Layout.fillWidth: true
            text: point.text
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
        }
        QQC2.Button {
            visible: point.action.length > 0
            enabled: point.actionEnabled
            text: point.action
            icon.name: point.actionIcon
            onClicked: point.triggered()
            Accessible.name: text
        }
    }
}

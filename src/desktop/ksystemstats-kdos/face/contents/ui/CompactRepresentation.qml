/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   CompactRepresentation.qml — KDOS Meters in a panel
 *
 * The panel's meter strip: each sensor as a sparkline and its figure, no
 * names, so three meters fit where one chart did. In a horizontal panel a
 * meter with several sensors puts them side by side, each marked by its
 * direction where it has one, and the applet is as wide as they are plus a
 * gap after the last figure, so a figure sits against its own line and apart
 * from the next meter's: the system monitor applet sizes itself from this
 * face's Layout hints.
 *
 * Its minimum is the strip without its lines. A panel holding more than its
 * screen's width shrinks every applet that has room to give toward its
 * minimum, and a strip that could not shrink would leave all of the deficit
 * to the task manager, whose launchers then shrink to dots. Given less than
 * its full width, the strip narrows its lines and keeps every figure.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Faces.CompactSensorFace {
    id: root

    readonly property real stripWidth: Math.ceil(meters.implicitWidth) + Kirigami.Units.largeSpacing
    readonly property real narrowWidth: root.stripWidth - Math.floor(meters.lineRoom)

    Layout.minimumWidth: root.horizontalFormFactor ? root.narrowWidth : Kirigami.Units.gridUnit * 5
    Layout.preferredWidth: root.horizontalFormFactor ? root.stripWidth : -1
    Layout.maximumWidth: root.horizontalFormFactor ? root.stripWidth : -1

    contentItem: Item {
        clip: true

        Scanlines {
            anchors.fill: parent
        }

        MeterList {
            id: meters
            anchors.fill: parent
            anchors.rightMargin: root.horizontalFormFactor ? Kirigami.Units.largeSpacing : 0
            controller: root.controller
            compact: true
            horizontal: root.horizontalFormFactor
        }
    }
}

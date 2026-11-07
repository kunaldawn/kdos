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
 * names, so three meters fit where one chart did.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Faces.CompactSensorFace {
    id: root

    Layout.minimumWidth: Kirigami.Units.gridUnit * 5

    contentItem: Item {
        Scanlines {
            anchors.fill: parent
        }

        MeterList {
            anchors.fill: parent
            controller: root.controller
            compact: true
        }
    }
}

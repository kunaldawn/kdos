/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   FullRepresentation.qml — KDOS Meters, the face as a page section shows it
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Faces.SensorFace {
    id: root

    Layout.minimumWidth: Kirigami.Units.gridUnit * 10
    Layout.preferredWidth: Kirigami.Units.gridUnit * 18

    contentItem: Item {
        implicitHeight: column.implicitHeight

        Scanlines {
            anchors.fill: parent
        }

        ColumnLayout {
            id: column
            anchors.fill: parent
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: root.controller.title
                visible: root.controller.showTitle && text.length > 0
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            MeterList {
                Layout.fillWidth: true
                controller: root.controller
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}

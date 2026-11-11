/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   FullRepresentation.qml — KDOS Meters, the face as a page section shows it
 *
 * The face asks for the height of its rows and for the width of its widest
 * narrowest row. System Monitor sizes a page row from its faces' minimum
 * heights and a desktop widget starts at the face's preferred size, so a face
 * that asked for nothing would be squeezed until its rows overlapped what
 * comes below. What still does not fit is clipped at the face's edge.
 *
 * Opened from a panel, the face is a popup, and a popup is a chart: wider,
 * each line four grid units high, and the lines share whatever height the
 * popup is given, so a popup made larger draws larger lines rather than
 * empty space below them.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Faces.SensorFace {
    id: root

    readonly property bool popup: root.formFactor === Faces.SensorFace.Horizontal || root.formFactor === Faces.SensorFace.Vertical

    contentItem: Item {
        implicitHeight: column.implicitHeight
        Layout.minimumWidth: meters.minimumRowWidth
        Layout.preferredWidth: Math.max(Kirigami.Units.gridUnit * (root.popup ? 24 : 18), meters.minimumRowWidth)
        Layout.minimumHeight: column.implicitHeight
        Layout.preferredHeight: column.implicitHeight
        clip: true

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
                id: meters
                Layout.fillWidth: true
                Layout.fillHeight: root.popup && meters.anyLine
                controller: root.controller
                tall: root.popup
            }

            Item {
                Layout.fillHeight: true
                visible: !(root.popup && meters.anyLine)
            }
        }
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   main.qml — the checklist's tray entry
 *
 * Shown while anything on the list is still to do, and gone from the tray,
 * hidden entries included, once every item is done or dismissed.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

import org.kdos.checklist

PlasmoidItem {
    id: root

    ChecklistModel {
        id: probe
    }

    Plasmoid.icon: "checkmark-symbolic"
    Plasmoid.title: i18ndc("kdos-plasmoid-checklist", "@title", "Getting Started")
    Plasmoid.status: !probe.ready || probe.total === 0 || probe.finished ? PlasmaCore.Types.HiddenStatus : PlasmaCore.Types.ActiveStatus

    toolTipMainText: i18ndc("kdos-plasmoid-checklist", "@info:tooltip", "Getting Started")
    toolTipSubText: i18ndcp("kdos-plasmoid-checklist", "@info:tooltip", "%1 thing left to do", "%1 things left to do", probe.remaining)

    fullRepresentation: PlasmaExtras.Representation {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.preferredHeight: Kirigami.Units.gridUnit * 24
        collapseMarginsHint: false

        header: PlasmaExtras.PlasmoidHeading {
            visible: !(Plasmoid.containmentDisplayHints & PlasmaCore.Types.ContainmentDrawsPlasmoidHeading)
            contentItem: Kirigami.Heading {
                level: 1
                text: i18ndc("kdos-plasmoid-checklist", "@title", "Getting Started")
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
        }

        /* A Representation with collapseMarginsHint false pads its content
         * by nothing, and the tray's popup sets the applet against its
         * frame, so the list keeps a large-spacing inset on every side or
         * its text runs flush to the frame; it narrows by the scroll bar's
         * width while the bar shows, or its rows would run under the bar. */
        contentItem: Flickable {
            id: flick

            readonly property int inset: Kirigami.Units.largeSpacing

            contentWidth: width
            contentHeight: list.implicitHeight + inset * 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            QQC2.ScrollBar.vertical: PlasmaComponents3.ScrollBar {
                id: bar
            }

            ChecklistView {
                id: list
                x: flick.inset
                y: flick.inset
                width: flick.width - flick.inset * 2 - (bar.visible ? bar.width : 0)
                checklist: probe
            }
        }
    }
}

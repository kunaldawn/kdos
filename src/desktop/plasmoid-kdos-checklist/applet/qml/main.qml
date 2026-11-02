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
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
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
            contentItem: Kirigami.Heading {
                level: 1
                text: i18ndc("kdos-plasmoid-checklist", "@title", "Getting Started")
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
        }

        contentItem: Flickable {
            contentHeight: list.implicitHeight
            clip: true

            ChecklistView {
                id: list
                width: parent.width
                checklist: probe
            }
        }
    }
}

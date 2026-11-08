/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   85-GettingStarted.qml — the first-run checklist, the tour's last page
 *
 * The same list as the Getting Started tray entry: ChecklistView from
 * plasmoid-kdos-checklist's org.kdos.checklist module, which makes its own
 * model over `kdos checklist --json`. A row ticks itself when the probe
 * finds the thing done, and "Not Now" here dismisses it in the tray too.
 * The tray entry stays until every item is done or dismissed.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import org.kdos.checklist

import "kdos" as Kdos

Welcome.ScrollablePage {
    id: page

    heading: i18ndc("kdos-welcome", "@title:window", "Getting Started")
    description: i18ndc("kdos-welcome", "@info:usagetip", "A few things worth doing on a new machine. Each one ticks itself when it is done, and the list stays in the system tray until then.")

    view: Kdos.Flick {
        ChecklistView {
            Layout.fillWidth: true
        }
    }
}

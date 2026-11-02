/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Flick.qml — a KDOS page's scrolling body
 *
 * Welcome.ScrollablePage scrolls the Flickable given as its `view`; this is
 * that Flickable, holding one column the page's items go into, so a page that
 * grows past the window (larger text, a narrow window) scrolls instead of
 * clipping its last controls.
 * ---------------------------------
 */

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

Flickable {
    id: flick

    default property alias content: column.data
    readonly property int gutter: Kirigami.Units.largeSpacing

    contentWidth: width
    contentHeight: column.implicitHeight + gutter * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: column

        x: flick.gutter
        y: flick.gutter
        width: flick.width - flick.gutter * 2
        spacing: Kirigami.Units.largeSpacing
    }
}

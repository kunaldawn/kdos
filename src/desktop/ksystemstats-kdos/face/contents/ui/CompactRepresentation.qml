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
 * direction where it has one, and the applet at its full width is as wide
 * as they are plus a gap after the last figure, so a figure sits against
 * its own line and apart from the next meter's: the system monitor applet
 * sizes itself from this face's Layout hints.
 *
 * Its preferred width is the strip without its lines, and it fills up to
 * the strip with them. A panel gives a filling applet only the room left
 * once every applet has its preferred width, so the task manager's
 * launchers are whole before any meter draws a line, and a panel short of
 * room takes the lines first. A panel that shrinks applets below their
 * preferred widths takes the most from whichever has the most to give, and
 * a strip that kept its lines there would leave the task manager's
 * launchers as dots.
 *
 * A strip whose stepOutBelow setting is more than 0 steps out of a
 * horizontal panel narrower than that many grid units: one pixel wide and
 * drawing nothing, so its room goes to the task manager. The width is the
 * panel window's own logical width, the room the applets share, and it is
 * read again whenever the panel is resized, by a change of display scale or
 * resolution as much as by a move to another screen. In a horizontal
 * panel the setting is read again from the applet's configuration at each
 * resize, before the width decides: the face's property map follows the
 * applet's configuration only when the applet reloads it, so a value
 * written there after the face loaded (as by a desk laid out in the
 * running session) takes effect at the next change of panel width. The
 * reload rebuilds every binding on faceConfiguration, so it is left out
 * of any other form factor, where the strip never steps out. The KDOS
 * desk's layout.js sets it to 82, the width of its full desk with six
 * tasks, on its network meter alone; a strip a person adds keeps the
 * default 0 and never steps out.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Faces.CompactSensorFace {
    id: root

    readonly property real stripWidth: Math.ceil(meters.implicitWidth) + Kirigami.Units.largeSpacing
    readonly property real narrowWidth: root.stripWidth - Math.floor(meters.lineRoom)
    readonly property int stepOutBelow: root.controller.faceConfiguration.stepOutBelow || 0
    readonly property bool stepsOut: root.horizontalFormFactor && root.stepOutBelow > 0 && Window.width > 0
                                     && Window.width < Kirigami.Units.gridUnit * root.stepOutBelow

    Connections {
        target: root.Window.window
        enabled: root.horizontalFormFactor
        function onWidthChanged() {
            root.controller.reloadFaceConfiguration();
        }
    }

    Layout.fillWidth: root.horizontalFormFactor && !root.stepsOut
    Layout.minimumWidth: !root.horizontalFormFactor ? Kirigami.Units.gridUnit * 5
                                                    : root.stepsOut ? 1 : root.narrowWidth
    Layout.preferredWidth: !root.horizontalFormFactor ? -1 : root.stepsOut ? 1 : root.narrowWidth
    Layout.maximumWidth: !root.horizontalFormFactor ? -1 : root.stepsOut ? 1 : root.stripWidth

    contentItem: Item {
        clip: true
        visible: !root.stepsOut

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

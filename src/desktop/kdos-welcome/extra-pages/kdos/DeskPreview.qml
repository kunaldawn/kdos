/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   DeskPreview.qml — a desk, drawn small in the running scheme
 *
 * A live picture rather than a screenshot: it is drawn from the colour roles
 * in force, so it changes with the accent, and it is drawn from the same
 * description the desk templates lay out (org.kdos.desk.*):
 *
 *   kdos    one opaque bottom panel: Start, desktops, pinned apps, meters,
 *           tray, clock
 *   dock    a thin top bar, and a floating dock of pinned apps
 *   tiling  a thinner top bar, and windows tiled edge to edge
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.kirigami as Kirigami

Rectangle {
    id: preview

    required property string desk

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    implicitWidth: Kirigami.Units.gridUnit * 10
    implicitHeight: Math.round(implicitWidth * 9 / 16)
    color: Kirigami.Theme.backgroundColor
    border.width: 1
    border.color: Kirigami.Theme.disabledTextColor
    clip: true

    Accessible.role: Accessible.Graphic
    Accessible.name: {
        switch (preview.desk) {
        case "dock":
            return i18ndc("kdos-welcome", "@info accessible picture", "A top bar, and a floating dock at the bottom");
        case "tiling":
            return i18ndc("kdos-welcome", "@info accessible picture", "A thin top bar, and windows tiled side by side");
        default:
            return i18ndc("kdos-welcome", "@info accessible picture", "One panel along the bottom of the screen");
        }
    }

    readonly property real u: width / 100
    readonly property color panel: Kirigami.Theme.alternateBackgroundColor
    readonly property color rule: Kirigami.Theme.highlightColor
    readonly property color ink: Kirigami.Theme.textColor
    readonly property color dim: Kirigami.Theme.disabledTextColor

    // A window: a title strip and a body, outlined in the focus colour.
    component Window: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: preview.rule

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 1
            }
            height: Math.max(2, preview.u * 3)
            color: preview.panel
        }
    }

    // A row of squares, the pinned apps or the tray.
    component Dots: Row {
        id: dots

        property int count: 4
        property real size: preview.u * 3
        property color tint: preview.ink
        spacing: preview.u * 1.5

        Repeater {
            model: dots.count

            delegate: Rectangle {
                width: dots.size
                height: dots.size
                color: dots.tint
            }
        }
    }

    // ── kdos ───────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: preview.desk === "kdos"

        Window {
            x: preview.u * 22
            y: preview.u * 8
            width: preview.u * 56
            height: preview.height - preview.u * 22
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: preview.u * 7
            color: preview.panel

            Rectangle {
                width: parent.width
                height: 1
                color: preview.rule
            }

            Rectangle {
                x: preview.u * 2
                anchors.verticalCenter: parent.verticalCenter
                width: preview.u * 8
                height: preview.u * 3
                color: preview.rule
            }
            Dots {
                x: preview.u * 13
                anchors.verticalCenter: parent.verticalCenter
                tint: preview.dim
            }
            Dots {
                x: preview.u * 34
                anchors.verticalCenter: parent.verticalCenter
                size: preview.u * 4
            }
            Dots {
                anchors {
                    right: parent.right
                    rightMargin: preview.u * 3
                    verticalCenter: parent.verticalCenter
                }
                count: 5
                size: preview.u * 2.5
            }
        }
    }

    // ── dock ───────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: preview.desk === "dock"

        Rectangle {
            width: parent.width
            height: preview.u * 4.5
            color: preview.panel

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: preview.rule
            }
            Rectangle {
                x: preview.u * 2
                anchors.verticalCenter: parent.verticalCenter
                width: preview.u * 2.5
                height: preview.u * 2.5
                color: preview.rule
            }
            Dots {
                x: preview.u * 7
                anchors.verticalCenter: parent.verticalCenter
                count: 3
                size: preview.u * 1.5
                tint: preview.dim
            }
            Dots {
                anchors {
                    right: parent.right
                    rightMargin: preview.u * 2
                    verticalCenter: parent.verticalCenter
                }
                count: 4
                size: preview.u * 2
            }
        }

        Window {
            x: preview.u * 18
            y: preview.u * 10
            width: preview.u * 64
            height: preview.height - preview.u * 26
        }

        Rectangle {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: preview.u * 2
            }
            width: preview.u * 36
            height: preview.u * 7
            color: preview.panel
            border.width: 1
            border.color: preview.rule

            Dots {
                anchors.centerIn: parent
                count: 5
                size: preview.u * 4
            }
        }
    }

    // ── tiling ─────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: preview.desk === "tiling"

        Rectangle {
            id: tilingBar

            width: parent.width
            height: preview.u * 3.5
            color: preview.panel

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: preview.rule
            }
            Dots {
                x: preview.u * 2
                anchors.verticalCenter: parent.verticalCenter
                count: 5
                size: preview.u * 2
                tint: preview.dim
            }
            Dots {
                anchors {
                    right: parent.right
                    rightMargin: preview.u * 2
                    verticalCenter: parent.verticalCenter
                }
                count: 3
                size: preview.u * 1.5
            }
        }

        Window {
            x: preview.u * 1
            y: tilingBar.height + preview.u * 1
            width: preview.u * 48.5
            height: preview.height - y - preview.u * 1
        }
        Window {
            x: preview.u * 50.5
            y: tilingBar.height + preview.u * 1
            width: preview.u * 48.5
            height: (preview.height - tilingBar.height - preview.u * 3) / 2
        }
        Window {
            x: preview.u * 50.5
            y: tilingBar.height + preview.u * 2 + (preview.height - tilingBar.height - preview.u * 3) / 2
            width: preview.u * 48.5
            height: (preview.height - tilingBar.height - preview.u * 3) / 2
        }
    }
}

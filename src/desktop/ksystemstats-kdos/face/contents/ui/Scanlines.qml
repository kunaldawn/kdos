/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Scanlines.qml — the faint ground of Phosphor mode
 *
 * Shown only while `kdos/look/phosphor` reads 1, which the KDOS sensor plugin
 * takes from kwinrc's `kdos_crtEnabled`. Lines in the accent at a few per
 * cent: a texture behind the figures, never a contrast loss on them.
 */

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.sensors as Sensors

Item {
    id: ground

    readonly property bool active: phosphor.value === 1

    visible: active
    clip: true
    Accessible.ignored: true

    Sensors.Sensor {
        id: phosphor
        sensorId: "kdos/look/phosphor"
    }

    Repeater {
        model: ground.active ? Math.ceil(ground.height / 3) : 0
        delegate: Rectangle {
            required property int index
            y: index * 3
            width: ground.width
            height: 1
            color: Kirigami.Theme.highlightColor
            opacity: 0.06
        }
    }
}

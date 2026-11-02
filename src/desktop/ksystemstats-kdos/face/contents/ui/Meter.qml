/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Meter.qml — one sensor: its name, a sparkline, its figure
 *
 * The name of a `kdos/<object>/<sensor>` row is its object's `name` sensor
 * when it has one, so a box's CPU row reads "Firefox (box www)" rather than
 * "CPU Usage" eight times over; a label set in the face's sensor settings
 * wins over both. The line is the accent, the figure is the monospace face.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.sensors as Sensors
import org.kde.quickcharts as Charts

RowLayout {
    id: meter

    property string sensorId
    property var controller
    property bool compact: false

    readonly property string label: {
        const own = meter.controller.sensorLabels[meter.sensorId];
        if (own) {
            return own;
        }
        if (nameSensor.sensorId.length > 0 && nameSensor.value !== undefined && String(nameSensor.value).length > 0) {
            return String(nameSensor.value);
        }
        return meter.compact ? sensor.shortName : sensor.name;
    }

    spacing: Kirigami.Units.smallSpacing

    Accessible.role: Accessible.StaticText
    Accessible.name: i18ndc("kdos-ksystemstats-kdos", "@info:accessible %1 sensor name, %2 its value", "%1: %2", meter.label, sensor.formattedValue)

    Sensors.Sensor {
        id: sensor
        sensorId: meter.sensorId
        updateRateLimit: meter.controller.updateRateLimit
    }

    Sensors.Sensor {
        id: nameSensor
        sensorId: meter.sensorId.startsWith("kdos/") && !meter.sensorId.endsWith("/name")
            ? meter.sensorId.replace(/\/[^\/]+$/, "/name") : ""
    }

    QQC2.Label {
        Layout.fillWidth: !sparkline.visible
        Layout.preferredWidth: Kirigami.Units.gridUnit * (meter.compact ? 3 : 9)
        Layout.minimumWidth: Kirigami.Units.gridUnit * 2
        visible: !meter.compact || !sparkline.visible
        text: meter.label
        elide: Text.ElideRight
        textFormat: Text.PlainText
        color: Kirigami.Theme.textColor
    }

    Charts.LineChart {
        id: sparkline

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Kirigami.Units.gridUnit
        Layout.preferredHeight: Kirigami.Units.gridUnit * 1.5
        visible: meter.controller.faceConfiguration.showSparklines
        Accessible.ignored: true

        direction: Charts.XYChart.ZeroAtEnd
        interpolate: true
        lineWidth: 1.5
        fillOpacity: 0.15
        smooth: true

        yRange {
            from: 0
            to: sensor.maximum > 0 ? sensor.maximum : 100
            automatic: !(sensor.maximum > 0)
        }

        colorSource: Charts.SingleValueSource {
            value: Kirigami.Theme.highlightColor
        }

        valueSources: Charts.HistoryProxySource {
            source: Charts.SingleValueSource {
                value: typeof sensor.value === "number" ? sensor.value : 0
            }
            interval: sensor.updateInterval > 0 ? sensor.updateInterval : 2000
            maximumHistory: Math.max(2, Math.round(meter.controller.faceConfiguration.historyAmount * 1000 / interval))
            fillMode: Charts.HistoryProxySource.FillFromEnd
        }
    }

    QQC2.Label {
        Layout.minimumWidth: Kirigami.Units.gridUnit * (meter.compact ? 3 : 5)
        horizontalAlignment: Text.AlignRight
        text: sensor.formattedValue
        textFormat: Text.PlainText
        font: Kirigami.Theme.fixedWidthFont
        color: Kirigami.Theme.textColor
    }
}

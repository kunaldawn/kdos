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
 *
 * The figure is as wide as the widest value its unit can format to, so it
 * never clips and its row never jitters as the value changes. In a panel a
 * meter drops its name; a direction sensor (network download and upload,
 * disk read and write) carries an arrow instead, so a two-sensor meter says
 * which figure is which. A panel line's least width is none, so a panel
 * short of room narrows the lines and keeps every arrow and figure whole; a
 * line narrower than a grid unit is not drawn.
 *
 * Out of a panel the line takes the width the name and the figure leave, so
 * a wide face draws a wide chart; the name keeps one width in every row, so
 * the lines of a stacked list start and end together. A `tall` meter, the
 * one a panel's popup shows, asks for a line four grid units high and
 * keeps its name and figure at the top of its row.
 *
 * The figure and the arrow are drawn by Qt, not natively: native text in the
 * panel strip can take colour fringes in one applet and not its neighbours,
 * and the strip's figures would then not match.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.formatter as Formatter
import org.kde.ksysguard.sensors as Sensors
import org.kde.quickcharts as Charts

RowLayout {
    id: meter

    property string sensorId
    property var controller
    property bool compact: false
    /* The width MeterList gives every figure of a stacked list, so the
     * figures, and the lines before them, line up from row to row. */
    property real columnFigureWidth: 0
    property bool tall: false

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

    /* Formatted from the value and the unit together: Sensor.formattedValue
     * is renotified only when the value changes, so a value that arrived
     * before its unit would stay formatted as unitless text, a raw float. */
    readonly property string figure: sensor.status === Sensors.Sensor.Ready ? Formatter.Formatter.formatValueShowNull(sensor.value, sensor.unit) : ""

    readonly property string marker: {
        if (/\/(download|downloadBits|read|ioRead)$/.test(meter.sensorId)) {
            return "\u2193";
        }
        if (/\/(upload|uploadBits|write|ioWrite)$/.test(meter.sensorId)) {
            return "\u2191";
        }
        return "";
    }

    /* A text sensor (a slot's status, a version) has no line, and its text
     * takes what the row leaves after the name, wrapping where it must. */
    readonly property bool textual: typeof sensor.value === "string"

    /* Whether this meter has a line at all. */
    readonly property bool drawsLine: meter.controller.faceConfiguration.showSparklines && !meter.textual
    /* A panel's line at its full width; MeterList adds these up. */
    readonly property real compactLineWidth: Math.round(Kirigami.Units.gridUnit * 2.5)

    /* A unit with no widest value (a count) takes its figure's own width, at
     * most eight grid units. A text figure's is only its least. */
    readonly property real figureWidth: {
        if (meter.textual) {
            return meter.figure.length > 0 ? Kirigami.Units.gridUnit * 4 : 0;
        }
        const widest = Formatter.Formatter.maximumLength(sensor.unit, figureLabel.font);
        const own = figureLabel.implicitWidth;
        return Math.ceil(widest > 0 ? Math.max(widest, own) : Math.min(own, Kirigami.Units.gridUnit * 8));
    }

    /* The narrowest row that still shows the name, the line and the whole
     * figure; MeterList takes the widest of these as the face's minimum. */
    readonly property real minimumRowWidth: (nameLabel.visible ? nameLabel.Layout.minimumWidth + spacing : 0)
        + (markerLabel.visible ? Math.ceil(markerLabel.implicitWidth) + spacing : 0)
        + (sparkline.visible ? sparkline.Layout.minimumWidth + spacing : 0)
        + figureWidth

    readonly property real shownFigureWidth: meter.textual ? figureWidth : Math.max(figureWidth, columnFigureWidth)

    spacing: Kirigami.Units.smallSpacing

    Accessible.role: Accessible.StaticText
    Accessible.name: i18ndc("kdos-ksystemstats-kdos", "@info:accessible %1 sensor name, %2 its value", "%1: %2", meter.label, meter.figure)

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
        id: nameLabel
        Layout.alignment: meter.tall ? Qt.AlignTop : Qt.AlignVCenter
        /* Filling, and beside a line no wider than its preferred width: a
         * layout holds an item that does not fill at exactly its preferred
         * width, so a name that did not fill would keep eight grid units in a
         * narrow card and push the figure out past the clip. */
        Layout.fillWidth: meter.textual ? meter.figure.length === 0 : true
        Layout.preferredWidth: meter.textual ? Math.min(implicitWidth, Kirigami.Units.gridUnit * 6) : Kirigami.Units.gridUnit * (meter.compact ? 3 : 8)
        Layout.maximumWidth: !meter.textual && sparkline.visible ? Layout.preferredWidth : Number.POSITIVE_INFINITY
        Layout.minimumWidth: Kirigami.Units.gridUnit * 2
        visible: !meter.compact || !meter.drawsLine
        text: meter.label
        elide: Text.ElideRight
        textFormat: Text.PlainText
        color: Kirigami.Theme.textColor
    }

    QQC2.Label {
        id: markerLabel
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredWidth: Math.ceil(implicitWidth)
        visible: meter.compact && meter.drawsLine && meter.marker.length > 0
        text: meter.marker
        renderType: Text.QtRendering
        textFormat: Text.PlainText
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.textColor
        Accessible.ignored: true
    }

    Charts.LineChart {
        id: sparkline

        Layout.fillHeight: true
        /* A layout sizes an item that does not fill to its preferred width
         * and never narrower, so the line fills in a panel too, up to its
         * maximum there. Out of a panel its preferred width is its least, so
         * a narrow row narrows the line to two grid units before it elides
         * the name, and a wide row gives the line all the name leaves. */
        Layout.fillWidth: true
        Layout.minimumWidth: meter.compact ? 0 : Kirigami.Units.gridUnit * 2
        Layout.preferredWidth: meter.compact ? meter.compactLineWidth : Kirigami.Units.gridUnit * 2
        Layout.maximumWidth: meter.compact ? Layout.preferredWidth : Number.POSITIVE_INFINITY
        Layout.minimumHeight: meter.compact ? 0 : Kirigami.Units.gridUnit
        Layout.preferredHeight: Kirigami.Units.gridUnit * (meter.tall ? 4 : 1.5)
        visible: meter.drawsLine
        opacity: width >= Kirigami.Units.gridUnit ? 1 : 0
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
            value: Kirigami.Theme.focusColor
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
        id: figureLabel
        Layout.alignment: meter.tall ? Qt.AlignTop : Qt.AlignVCenter
        Layout.fillWidth: meter.textual
        Layout.minimumWidth: meter.shownFigureWidth
        Layout.preferredWidth: meter.textual ? implicitWidth : meter.shownFigureWidth
        Layout.maximumWidth: meter.textual ? Number.POSITIVE_INFINITY : meter.shownFigureWidth
        horizontalAlignment: meter.compact ? Text.AlignLeft : Text.AlignRight
        wrapMode: meter.textual && !meter.compact ? Text.Wrap : Text.NoWrap
        text: meter.figure
        renderType: Text.QtRendering
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: meter.compact ? Kirigami.Theme.smallFont.pointSize : Kirigami.Theme.fixedWidthFont.pointSize
        color: Kirigami.Theme.textColor
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   MeterList.qml — which sensors get a row
 *
 * Either the sensors the face was given, or, when the face's `sensorPattern`
 * is set, every sensor whose id matches it as the daemon adds and removes
 * them. The face controller resolves a pattern in the sensor list once, when
 * the face loads, so a box started afterwards would never get a row; the
 * pattern here is matched against the live sensor tree instead, so a box's
 * row comes and goes with the box.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

import org.kde.ksysguard.sensors as Sensors

ColumnLayout {
    id: list

    property var controller
    property bool compact: false

    readonly property string pattern: list.controller.faceConfiguration.sensorPattern
    readonly property var matcher: {
        if (list.pattern.length === 0) {
            return null;
        }
        try {
            return new RegExp(list.pattern);
        } catch (e) {
            return null;
        }
    }
    readonly property int rows: repeater.count

    spacing: Kirigami.Units.smallSpacing

    Sensors.SensorTreeModel {
        id: tree
    }

    KItemModels.KDescendantsProxyModel {
        id: flat
        model: list.matcher ? tree : null
        expandsByDefault: true
    }

    KItemModels.KSortFilterProxyModel {
        id: matching
        sourceModel: list.matcher ? flat : null
        sortRoleName: "SensorId"
        filterRowCallback: (row, parent) => {
            const id = flat.data(flat.index(row, 0, parent), Sensors.SensorTreeModel.SensorId);
            return typeof id === "string" && id.length > 0 && list.matcher !== null && list.matcher.test(id);
        }
        onSourceModelChanged: invalidateFilter()
    }

    onMatcherChanged: matching.invalidateFilter()

    Repeater {
        id: repeater
        model: list.matcher ? matching : list.controller.highPrioritySensorIds
        delegate: Meter {
            required property int index
            required property var model
            Layout.fillWidth: true
            visible: index < list.controller.faceConfiguration.maximumRows
            controller: list.controller
            compact: list.compact
            sensorId: list.matcher ? model.SensorId : model.modelData
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: repeater.count === 0
        text: list.matcher ? i18ndc("kdos-ksystemstats-kdos", "@info face with a pattern that matches no sensor yet", "Nothing to show yet")
                           : i18ndc("kdos-ksystemstats-kdos", "@info face with no sensors", "No sensors chosen")
        horizontalAlignment: Text.AlignHCenter
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}

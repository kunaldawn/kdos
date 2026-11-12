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
 *
 * The meters stack, one row each, except in a horizontal panel, where they
 * stand side by side: a panel is one row high, and stacked meters there
 * would share its height and run past it. A `tall` list's line rows share
 * the height the list is given, so a popup's chart grows with the popup.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

import org.kde.ksysguard.sensors as Sensors

GridLayout {
    id: list

    property var controller
    property bool compact: false
    property bool horizontal: false
    property bool tall: false

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
    readonly property int count: repeater.count

    /* The widest of the shown meters' narrowest rows. */
    readonly property real minimumRowWidth: {
        let widest = Kirigami.Units.gridUnit * 4;
        for (let i = 0; i < repeater.count; ++i) {
            const m = repeater.itemAt(i) as Meter;
            if (m && m.visible) {
                widest = Math.max(widest, m.minimumRowWidth);
            }
        }
        return Math.ceil(widest);
    }

    /* The width the shown meters' lines take in a panel at their full
     * width: what a panel short of room may take back. */
    readonly property real lineRoom: {
        let room = 0;
        for (let i = 0; i < repeater.count; ++i) {
            const m = repeater.itemAt(i) as Meter;
            if (m && m.visible && m.drawsLine) {
                room += m.compactLineWidth;
            }
        }
        return room;
    }

    /* Whether any shown meter draws a line, which a tall list grows. */
    readonly property bool anyLine: lineRoom > 0

    /* The widest number of the shown meters, which a stacked list gives to
     * every number. */
    readonly property real figureWidth: {
        let widest = 0;
        for (let i = 0; i < repeater.count; ++i) {
            const m = repeater.itemAt(i) as Meter;
            if (m && m.visible && !m.textual) {
                widest = Math.max(widest, m.figureWidth);
            }
        }
        return widest;
    }

    flow: list.horizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom
    rowSpacing: Kirigami.Units.smallSpacing
    columnSpacing: Kirigami.Units.largeSpacing

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
            /* Filling, so a panel short of room can narrow a meter; side by
             * side a meter is never wider than its preferred width. */
            Layout.fillWidth: true
            Layout.maximumWidth: list.horizontal ? implicitWidth : Number.POSITIVE_INFINITY
            Layout.fillHeight: list.horizontal || (list.tall && drawsLine)
            visible: index < list.controller.faceConfiguration.maximumRows
            controller: list.controller
            compact: list.compact
            tall: list.tall
            columnFigureWidth: list.horizontal ? 0 : list.figureWidth
            sensorId: list.matcher ? model.SensorId : model.modelData
        }
    }

    QQC2.Label {
        Layout.fillWidth: !list.horizontal
        visible: repeater.count === 0
        text: list.matcher ? i18ndc("kdos-ksystemstats-kdos", "@info face with a pattern that matches no sensor yet", "Nothing to show yet")
                           : i18ndc("kdos-ksystemstats-kdos", "@info face with no sensors", "No sensors chosen")
        horizontalAlignment: Text.AlignHCenter
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}

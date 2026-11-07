/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Config.qml — KDOS Meters' own settings
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami

import org.kde.ksysguard.faces as Faces

Kirigami.FormLayout {
    id: root

    property alias cfg_showSparklines: sparklines.checked
    property alias cfg_historyAmount: history.value
    property alias cfg_maximumRows: rows.value
    property alias cfg_sensorPattern: pattern.text

    QQC2.CheckBox {
        id: sparklines
        Kirigami.FormData.label: i18ndc("kdos-ksystemstats-kdos", "@title:group", "Appearance:")
        text: i18ndc("kdos-ksystemstats-kdos", "@option:check", "Show sparklines")
        Accessible.name: text
    }

    Faces.SuffixSpinBox {
        id: history
        Kirigami.FormData.label: i18ndc("kdos-ksystemstats-kdos", "@label:spinbox", "History to show:")
        Layout.maximumWidth: Kirigami.Units.gridUnit * 10
        from: 10
        to: 3600
        suffix: i18ndcp("kdos-ksystemstats-kdos", "@item:valuesuffix %1 is seconds of history", "second", "seconds", value)
        Accessible.name: i18ndc("kdos-ksystemstats-kdos", "@label:spinbox accessible", "Seconds of history to show")
    }

    QQC2.SpinBox {
        id: rows
        Kirigami.FormData.label: i18ndc("kdos-ksystemstats-kdos", "@label:spinbox", "Rows at most:")
        from: 1
        to: 64
        Accessible.name: i18ndc("kdos-ksystemstats-kdos", "@label:spinbox accessible", "Most rows to show")
    }

    QQC2.TextField {
        id: pattern
        Kirigami.FormData.label: i18ndc("kdos-ksystemstats-kdos", "@label:textbox", "Follow sensors matching:")
        Layout.preferredWidth: Kirigami.Units.gridUnit * 16
        font: Kirigami.Theme.fixedWidthFont
        placeholderText: i18ndc("kdos-ksystemstats-kdos", "@info:placeholder", "kdos/box/(?!all/).*/cpu$")
        Accessible.name: i18ndc("kdos-ksystemstats-kdos", "@label:textbox accessible", "Regular expression of the sensors to follow")
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 20
        text: i18ndc("kdos-ksystemstats-kdos", "@info", "With a pattern, the face shows every sensor whose id matches it, as sensors appear and go, instead of the sensors chosen above.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
    }
}

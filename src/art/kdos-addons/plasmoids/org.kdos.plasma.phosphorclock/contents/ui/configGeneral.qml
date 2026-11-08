/*
 * The Phosphor Clock's settings page.
 *
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    property alias cfg_showSeconds: showSeconds.checked
    property alias cfg_use24Hour: use24Hour.checked
    property alias cfg_blinkSeparator: blinkSeparator.checked
    property alias cfg_showDate: showDate.checked

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: showSeconds
            Kirigami.FormData.label: i18nc("@title:group", "Show:")
            text: i18nc("@option:check", "Seconds")
            Accessible.name: i18nc("@option:check accessible", "Show the seconds")
        }
        QQC2.CheckBox {
            id: showDate
            text: i18nc("@option:check", "Date in the panel")
            Accessible.name: i18nc("@option:check accessible", "Show the date under the time in the panel")
        }
        QQC2.CheckBox {
            id: use24Hour
            Kirigami.FormData.label: i18nc("@title:group", "Clock:")
            text: i18nc("@option:check", "24-hour")
            Accessible.name: i18nc("@option:check accessible", "Use a 24-hour clock")
        }
        QQC2.CheckBox {
            id: blinkSeparator
            text: i18nc("@option:check", "Blink the separator")
            Accessible.name: i18nc("@option:check accessible", "Blink the separator once a second")
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: blinkSeparator.checked
            text: i18nc("@info", "The separator stays still while reduced motion is on.")
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
        }
    }
}

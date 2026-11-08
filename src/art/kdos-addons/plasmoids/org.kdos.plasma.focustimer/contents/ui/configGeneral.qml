/*
 * The Focus Timer's settings page.
 *
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    property alias cfg_focusMinutes: focusMinutes.value
    property alias cfg_shortBreakMinutes: shortBreakMinutes.value
    property alias cfg_longBreakMinutes: longBreakMinutes.value
    property alias cfg_longBreakEvery: longBreakEvery.value
    property alias cfg_autoStartNext: autoStartNext.checked

    Kirigami.FormLayout {
        QQC2.SpinBox {
            id: focusMinutes
            Kirigami.FormData.label: i18nc("@label:spinbox", "Focus:")
            from: 1
            to: 180
            textFromValue: (value, locale) => i18ncp("@item:valuesuffix", "%1 minute", "%1 minutes", value)
            valueFromText: (text, locale) => parseInt(text, 10)
            Accessible.name: i18nc("@label:spinbox accessible", "Focus period in minutes")
        }
        QQC2.SpinBox {
            id: shortBreakMinutes
            Kirigami.FormData.label: i18nc("@label:spinbox", "Short break:")
            from: 1
            to: 60
            textFromValue: (value, locale) => i18ncp("@item:valuesuffix", "%1 minute", "%1 minutes", value)
            valueFromText: (text, locale) => parseInt(text, 10)
            Accessible.name: i18nc("@label:spinbox accessible", "Short break in minutes")
        }
        QQC2.SpinBox {
            id: longBreakMinutes
            Kirigami.FormData.label: i18nc("@label:spinbox", "Long break:")
            from: 1
            to: 120
            textFromValue: (value, locale) => i18ncp("@item:valuesuffix", "%1 minute", "%1 minutes", value)
            valueFromText: (text, locale) => parseInt(text, 10)
            Accessible.name: i18nc("@label:spinbox accessible", "Long break in minutes")
        }
        QQC2.SpinBox {
            id: longBreakEvery
            Kirigami.FormData.label: i18nc("@label:spinbox", "Long break after:")
            from: 2
            to: 12
            textFromValue: (value, locale) => i18ncp("@item:valuesuffix", "%1 focus period", "%1 focus periods", value)
            valueFromText: (text, locale) => parseInt(text, 10)
            Accessible.name: i18nc("@label:spinbox accessible", "Focus periods before a long break")
        }
        QQC2.CheckBox {
            id: autoStartNext
            Kirigami.FormData.label: i18nc("@title:group", "When a period ends:")
            text: i18nc("@option:check", "Start the next one")
            Accessible.name: i18nc("@option:check accessible", "Start the next period without waiting")
        }
    }
}

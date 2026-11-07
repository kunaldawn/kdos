/*
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("General")
        icon: "chronometer"
        source: "configGeneral.qml"
    }
}

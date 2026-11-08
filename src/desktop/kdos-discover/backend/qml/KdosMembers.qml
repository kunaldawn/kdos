/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — what a set or a .ktar installs
 * ---------------------------------
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.discover as Discover
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property Discover.AbstractResource resource

    Discover.Activatable.active: root.resource !== null && (root.resource.memberNames ?? []).length > 0
    spacing: Kirigami.Units.smallSpacing

    Kirigami.Heading {
        Layout.fillWidth: true
        text: i18ndc("kdos-discover", "@title:group the applications a set installs", "In this set")
        level: 2
        type: Kirigami.Heading.Type.Primary
        wrapMode: Text.Wrap
        Accessible.name: text
    }

    Repeater {
        model: root.resource?.memberNames ?? []

        delegate: QQC2.Label {
            required property string modelData

            Layout.fillWidth: true
            text: modelData
            wrapMode: Text.Wrap
            Accessible.name: modelData
        }
    }
}

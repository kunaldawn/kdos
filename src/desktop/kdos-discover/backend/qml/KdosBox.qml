/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — "Runs in a box", on every resource that has one
 *
 * What the box is made of and how it is kept, as `kdos-box list` reported
 * it; what it is allowed, as `kdos-box profile` reports it enforced, asked
 * for when the page shows; and the way to change that: the Boxes page in
 * System Settings (kcm_kdos_boxes), opened on this box. Discover itself
 * changes no grant.
 * ---------------------------------
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.discover as Discover
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property Discover.AbstractResource resource

    Discover.Activatable.active: root.resource !== null && (root.resource.boxName ?? "").length > 0
    spacing: Kirigami.Units.smallSpacing

    Component.onCompleted: {
        if (root.resource && (root.resource.boxName ?? "").length > 0)
            root.resource.fetchBoxGrants()
    }

    Kirigami.Heading {
        Layout.fillWidth: true
        text: i18ndc("kdos-discover", "@title:group", "Runs in a box")
        level: 2
        type: Kirigami.Heading.Type.Primary
        wrapMode: Text.Wrap
        Accessible.name: text
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Kirigami.FormData.label: i18ndc("kdos-discover", "@label the name of a box", "Box:")
            text: root.resource?.boxName ?? ""
            Accessible.name: i18ndc("kdos-discover", "@info:whatsthis", "Box name: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18ndc("kdos-discover", "@label what a box is built on", "Made from:")
            text: root.resource?.boxBase ?? ""
            Accessible.name: i18ndc("kdos-discover", "@info:whatsthis", "Made from: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18ndc("kdos-discover", "@label whether a box is running", "State:")
            text: root.resource?.boxState ?? ""
            Accessible.name: i18ndc("kdos-discover", "@info:whatsthis", "State: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18ndc("kdos-discover", "@label the accent colour a box is shown in", "Colour:")
            text: root.resource?.boxAccent ?? ""
            Accessible.name: i18ndc("kdos-discover", "@info:whatsthis", "Colour: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18ndc("kdos-discover", "@label whether a box keeps what it writes", "Keeps its files:")
            text: root.resource?.boxPersistence ?? ""
            Accessible.name: i18ndc("kdos-discover", "@info:whatsthis", "Keeps its files: %1", text)
        }
    }

    Kirigami.Heading {
        Layout.fillWidth: true
        visible: (root.resource?.boxGrants ?? []).length > 0
        text: i18ndc("kdos-discover", "@title:group what a box is allowed", "Permissions")
        level: 3
        wrapMode: Text.Wrap
        Accessible.name: text
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true
        visible: (root.resource?.boxGrants ?? []).length > 0

        Repeater {
            model: root.resource?.boxGrants ?? []
            delegate: QQC2.Label {
                required property var modelData
                Kirigami.FormData.label: modelData.label
                text: modelData.value
                wrapMode: Text.Wrap
                Accessible.name: i18ndc("kdos-discover", "@info:whatsthis a box's permission and its value", "%1 %2", modelData.label, modelData.value)
            }
        }
    }

    Repeater {
        model: root.resource?.boxWarnings ?? []
        delegate: Kirigami.InlineMessage {
            required property string modelData
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Warning
            text: modelData
            Accessible.name: modelData
        }
    }

    QQC2.Button {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: root.width
        text: i18ndc("kdos-discover", "@action:button", "Box Settings…")
        icon.name: "configure"
        Accessible.name: i18ndc("kdos-discover", "@action:button", "Open the settings of the box %1", root.resource?.boxName ?? "")
        onClicked: KCMUtils.KCMLauncher.openSystemSettings("kcm_kdos_boxes", [root.resource.boxName])
    }
}

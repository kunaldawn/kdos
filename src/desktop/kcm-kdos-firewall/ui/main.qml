/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Firewall — which of this computer's services answer the network
 *
 * THE POLICY IS ON THE PAGE. Every row is an exception to it, and a list of
 * exceptions with the rule missing reads as the whole policy: an empty list
 * would look like "nothing is protected" rather than "nothing is opened".
 *
 * OPEN IS THE WARNING COLOUR, not the accent. A service that answers the
 * network is the state worth noticing, and the accent would read as "this
 * one is fine".
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD

KCM.ScrollViewKCM {
    id: root

    readonly property var controller: kcm.controller

    implicitWidth: Kirigami.Units.gridUnit * 36
    implicitHeight: Kirigami.Units.gridUnit * 30

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: root.controller.errorMessage
            visible: text.length > 0
            showCloseButton: true
            onVisibleChanged: {
                if (!visible) {
                    root.controller.dismissError();
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: root.controller.readOnlyReason
            visible: text.length > 0
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: root.controller.available
            wrapMode: Text.Wrap
            text: i18ndc("kdos-kcm-firewall", "@info the firewall's default policy", "Everything else arriving from the network is turned away. Replies to connections this computer made, and ping, always get in.")
        }
    }

    view: ListView {
        id: list

        model: root.controller.services
        reuseItems: true
        activeFocusOnTab: true
        Accessible.name: i18ndc("kdos-kcm-firewall", "@label accessible name of the list", "Services")

        delegate: QQC2.SwitchDelegate {
            id: row

            required property int index
            required property string name
            required property string description
            required property bool open
            required property double knocks

            width: ListView.view.width
            text: name
            checked: open
            enabled: root.controller.canChange && !root.controller.applying

            Accessible.name: i18ndc("kdos-kcm-firewall", "@action:button %1 is a firewall service name", "Let %1 through the firewall", name)
            Accessible.description: description

            onToggled: {
                root.controller.setServiceOpen(index, checked);
                checked = Qt.binding(() => row.open);
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                KD.TitleSubtitle {
                    Layout.fillWidth: true
                    title: row.name
                    subtitle: row.knocks > 0
                        ? i18ndcp("kdos-kcm-firewall", "@info %2 is what the service is for", "%2 — turned away %1 connection attempt", "%2 — turned away %1 connection attempts", row.knocks, row.description)
                        : row.description
                    selected: row.down
                    font: row.font
                }

                QQC2.Label {
                    text: row.open ? i18ndc("kdos-kcm-firewall", "@info:status a firewall service", "Open") : i18ndc("kdos-kcm-firewall", "@info:status a firewall service", "Closed")
                    color: row.open ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor
                    Accessible.ignored: true
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: !root.controller.loading && !root.controller.available
            icon.name: "action-unavailable-symbolic"
            text: i18ndc("kdos-kcm-firewall", "@info", "The firewall cannot be shown")
            explanation: root.controller.unavailableReason
            helpfulAction: Kirigami.Action {
                icon.name: "view-refresh"
                text: i18ndc("kdos-kcm-firewall", "@action:button", "Try Again")
                onTriggered: root.controller.load()
            }
        }

        QQC2.BusyIndicator {
            anchors.centerIn: parent
            visible: root.controller.loading && list.count === 0
            running: visible
            Accessible.name: i18ndc("kdos-kcm-firewall", "@info accessible name", "Loading the firewall")
        }
    }

    footer: QQC2.Label {
        visible: root.controller.available
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        text: i18ndc("kdos-kcm-firewall", "@info", "This page rewrites /etc/nftables.d/50-kdos-services.nft whole. Rules written by hand belong in another file in that folder, which it never reads or changes.")
    }
}

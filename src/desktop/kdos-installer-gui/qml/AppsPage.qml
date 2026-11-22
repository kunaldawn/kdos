/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   AppsPage — which applications the new system gets, and how
 *
 * Two kinds, as kinstall reads them. The native groups (/ports/groups) are
 * downloaded prebuilt into the new system during the install when the
 * medium names a binary host and the network answers, and recorded for
 * `kdos update install --pending` otherwise; the header and each row's
 * accessible name say which, from the plan's `binhost` and `network`, and
 * call it a binary host in every variant, as the Summary and the terminal
 * installer do. The header puts the command on a line of its own. The page
 * asks for the plan as it opens (Backend.checkPlan): the Disk page asks only
 * on the erase plan, and the Summary's comes after this page. Until the plan
 * answers, the fact is unknown and the text names both routes, since a
 * missing key read as "no binary host" would tell a medium that names one it
 * downloads nothing. The language group the keyboard or the time zone points
 * at starts ticked, and says so; each names how many ports it installs, a
 * meta-port member counted by kinstall as the ports it depends on. The
 * catalogue's groups are boxed applications, recorded and offered by the
 * first login, which builds them. The page says how each arrives, because a
 * person must not find out at the first login that nothing was installed.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Applications")
    readonly property var answers: Backend.answers
    readonly property var groups: Backend.apps.groups || []
    readonly property var nativeGroups: Backend.apps.native || []

    /* true, false, or undefined while the plan has not said. */
    readonly property var binhost: Backend.plan.binhost
    /* A binary host named, and no network to reach it over: the groups
     * are recorded and installed after the first boot. */
    readonly property bool offline: binhost === true && Backend.plan.network === false

    onEntered: {
        Backend.refreshApps();
        Backend.checkPlan();
    }

    Kirigami.Heading {
        level: 3
        visible: page.nativeGroups.length > 0
        text: i18nc("@title:group", "KDOS applications")
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: page.nativeGroups.length > 0
        wrapMode: Text.Wrap
        /* With no binary host named, or no network, nothing is downloaded
         * during the install: the header says what the Summary page says,
         * before the choice. The command stands on a line of its own, so the label's
         * wrapping never splits it at a space. */
        text: page.offline
            ? i18n("Native programs. No network was found, so the install only records the groups chosen here, and this installs them after the first boot once there is a network:\nsudo kdos update install --pending")
            : page.binhost === true
            ? i18n("Native programs, downloaded prebuilt into the new system during the install. This needs a network; without one the choice is kept, and this installs it later:\nsudo kdos update install --pending")
            : page.binhost === false
            ? i18n("Native programs. This system names no binary host, so the install only records the groups chosen here, and this installs them once a binary host answers:\nsudo kdos update install --pending")
            : i18n("Native programs, downloaded prebuilt into the new system during the install when this system names a binary host and the network answers; otherwise the choice is kept, and this installs it later:\nsudo kdos update install --pending")
    }

    Repeater {
        model: page.nativeGroups
        delegate: QQC2.CheckDelegate {
            id: nativeGroup
            required property var modelData
            Layout.fillWidth: true
            /* A delegate takes no focus by default, which leaves a
             * keyboard no way to tick the group. */
            focusPolicy: Qt.StrongFocus
            /* The keyboard's place, framed: the delegate's own focus is a
             * faint shade beside the tick, which a ticked row hides. */
            Rectangle {
                anchors.fill: parent
                z: 1
                visible: nativeGroup.visualFocus
                color: "transparent"
                border.color: Kirigami.Theme.focusColor
                border.width: 2
                radius: Kirigami.Units.cornerRadius
            }
            text: modelData.title || modelData.id
            checked: page.answers.groups.indexOf(modelData.id) >= 0
            /* Why a language group is ticked nobody ticked: the
             * keyboard or the time zone chosen earlier points at it. */
            readonly property string why: modelData.why
                ? (checked ? i18nc("@info %1 the Japanese keyboard", "Ticked for %1", modelData.why)
                           : i18nc("@info %1 the Japanese keyboard", "Suggested for %1", modelData.why))
                : ""
            /* With no binary host named, or no network, the install only
             * records the group. */
            readonly property string fate: page.offline
                ? i18n("recorded; installed after the first boot once there is a network")
                : page.binhost === true
                ? i18n("downloaded during the install")
                : page.binhost === false
                ? i18n("recorded; sudo kdos update install --pending installs it once a binary host answers")
                : i18n("downloaded during the install when a binary host answers, recorded otherwise")
            Accessible.name: why
                ? i18ncp("@info:whatsthis %2 group %1 count %3 fate %4 reason", "%2, %1 port, %3; %4",
                         "%2, %1 ports, %3; %4", modelData.ports || 0, text, fate, why)
                : i18ncp("@info:whatsthis %2 group %1 count %3 fate", "%2, %1 port, %3",
                         "%2, %1 ports, %3", modelData.ports || 0, text, fate)
            onToggled: {
                page.answers.setGroupChosen(modelData.id, checked);
                Backend.refreshApps();
            }
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                QQC2.Label {
                    Layout.fillWidth: true
                    text: nativeGroup.text
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    visible: nativeGroup.why.length > 0
                    text: nativeGroup.why
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    visible: nativeGroup.modelData.ports !== undefined
                    text: i18ncp("@info", "%1 port", "%1 ports", nativeGroup.modelData.ports || 0)
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }

    Kirigami.Heading {
        level: 3
        visible: page.groups.length > 0
        text: i18nc("@title:group", "Boxed applications")
    }

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: page.groups.length > 0
            ? i18n("Applications KDOS does not port, or ports at an older version, built from the catalogue, each in a box of its own, after the install.")
            : i18n("This medium carries no application catalogue. Applications are installed later from Discover.")
    }

    Repeater {
        model: page.groups
        delegate: QQC2.CheckDelegate {
            id: group
            required property var modelData
            Layout.fillWidth: true
            /* A delegate takes no focus by default, which leaves a
             * keyboard no way to tick the group. */
            focusPolicy: Qt.StrongFocus
            /* The keyboard's place, framed: the delegate's own focus is a
             * faint shade beside the tick, which a ticked row hides. */
            Rectangle {
                anchors.fill: parent
                z: 1
                visible: group.visualFocus
                color: "transparent"
                border.color: Kirigami.Theme.focusColor
                border.width: 2
                radius: Kirigami.Units.cornerRadius
            }
            text: modelData.desc || modelData.id
            checked: page.answers.apps.indexOf(modelData.id) >= 0
            Accessible.name: i18nc("@info:whatsthis %1 group %2 count %3 size", "%1, %2 applications, about %3",
                                   text, modelData.apps, Backend.formatSize(modelData.bytes))
            onToggled: {
                page.answers.setAppChosen(modelData.id, checked);
                Backend.refreshApps();
            }
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                QQC2.Label {
                    Layout.fillWidth: true
                    text: group.text
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    text: i18ncp("@info", "%1 app", "%1 apps", group.modelData.apps)
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    text: i18nc("@info an estimate", "~%1", Backend.formatSize(group.modelData.bytes))
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: page.groups.length > 0
        type: Kirigami.MessageType.Information
        text: Backend.apps.route === "pending"
            ? i18n("The choice is recorded on the new disk, and the first login offers to build them; that needs a network and takes minutes per application. Nothing is built during the install.")
            : i18n("No group is chosen; Discover installs applications later.")
    }

    QQC2.CheckBox {
        text: i18nc("@option:check", "Keep the box images this live session has")
        checked: page.answers.alienApps
        onToggled: page.answers.alienApps = checked
    }
    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit * 2
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        text: i18n("The container images the live session pulled or built, usually none: the medium ships no prebuilt images.")
    }
}

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
 * network answers, and recorded for `kdos update install --pending` when it
 * does not; the language group the keyboard or the time zone points at
 * starts ticked, and says so. The catalogue's groups are boxed applications, recorded and
 * offered by the first login, which builds them. The page says how each
 * arrives, because a person must not find out at the first login that
 * nothing was installed.
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

    onEntered: Backend.refreshApps()

    Kirigami.Heading {
        level: 3
        visible: page.nativeGroups.length > 0
        text: i18nc("@title:group", "KDOS applications")
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: page.nativeGroups.length > 0
        wrapMode: Text.Wrap
        text: i18n("Native programs, downloaded prebuilt into the new system during the install. This needs a network; without one the choice is kept, and “kdos update install --pending” installs it later.")
    }

    Repeater {
        model: page.nativeGroups
        delegate: QQC2.CheckDelegate {
            id: nativeGroup
            required property var modelData
            Layout.fillWidth: true
            text: modelData.title || modelData.id
            checked: page.answers.groups.indexOf(modelData.id) >= 0
            /* Why a language group is ticked nobody ticked: the
             * keyboard or the time zone chosen earlier points at it. */
            readonly property string why: modelData.why
                ? (checked ? i18nc("@info %1 the Japanese keyboard", "Ticked for %1", modelData.why)
                           : i18nc("@info %1 the Japanese keyboard", "Suggested for %1", modelData.why))
                : ""
            Accessible.name: why
                ? i18nc("@info:whatsthis %1 group %2 reason", "%1, downloaded during the install; %2", text, why)
                : i18nc("@info:whatsthis %1 group", "%1, downloaded during the install", text)
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

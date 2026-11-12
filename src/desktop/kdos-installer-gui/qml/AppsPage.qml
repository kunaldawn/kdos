/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   AppsPage — which application groups the machine builds, and how
 *
 * The groups are the medium's catalogue, as kinstall reads it. The page says
 * how the ticked ones arrive — recorded, and offered by the first login,
 * which builds them; nothing is built during the install — because a person
 * must not find out at the first login that nothing was installed.
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

    onEntered: Backend.refreshApps()

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: page.groups.length > 0
            ? i18n("KDOS's own applications are on the disk already. These groups are built from the store, each in a box of its own, after the install.")
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

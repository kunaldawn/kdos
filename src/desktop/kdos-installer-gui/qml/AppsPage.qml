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
 * how the ticked ones will arrive — from a stick, built over the network
 * during the install, or offered at the first login — because a person must
 * not find out at the first login that nothing was installed.
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
            ? i18n("KDOS's own applications are on the disk already. These groups are built from the store, each in a box of its own.")
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
        type: Backend.apps.route === "pending" ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
        text: Backend.apps.route === "import" ? i18n("They come from the set on %1 — no network needed.", Backend.apps.archive)
            : Backend.apps.route === "network" ? i18n("They are built during the install, over the network; this adds minutes to it.")
            : Backend.apps.route === "pending" ? i18n("There is no network and no exported set, so the choice is kept and the first login offers to build them.")
            : i18n("No group is chosen; the store installs them later.")
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

/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * One root slot: what is in it, what state it is in, and the button that
 * starts it next time. The button exists only on the slot that is neither
 * running nor already armed, so the page never offers to start the version
 * the computer is already on.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    id: form

    required property var slotInfo
    required property bool busy

    signal startRequested(var slotInfo)

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: form.slotInfo.title
    }

    QQC2.Label {
        Kirigami.FormData.label: i18nc("@label slot state", "State:")
        Layout.fillWidth: true
        text: form.slotInfo.stateText
        wrapMode: Text.Wrap
        color: form.slotInfo.failed ? Kirigami.Theme.negativeTextColor
             : form.slotInfo.trial ? Kirigami.Theme.neutralTextColor
             : Kirigami.Theme.textColor
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 state", "%1 state: %2", form.slotInfo.title, text)
    }

    Kirigami.SelectableLabel {
        Kirigami.FormData.label: i18nc("@label", "Version:")
        text: form.slotInfo.versionText
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 version", "%1 version: %2", form.slotInfo.title, text)
    }

    Kirigami.SelectableLabel {
        Kirigami.FormData.label: i18nc("@label when the system was built", "Built:")
        visible: form.slotInfo.present
        text: form.slotInfo.buildTimeText
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 date", "%1 built: %2", form.slotInfo.title, text)
    }

    Kirigami.SelectableLabel {
        id: commitLabel
        Kirigami.FormData.label: i18nc("@label the commit of the recipe tree the system was built from", "Tree commit:")
        visible: form.slotInfo.present
        text: form.slotInfo.shortCommit
        font: Kirigami.Theme.fixedWidthFont
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 commit", "%1 tree commit: %2", form.slotInfo.title, form.slotInfo.treeCommit)

        HoverHandler {
            id: commitHover
        }
        QQC2.ToolTip.visible: commitHover.hovered && form.slotInfo.treeCommit.length > 12
        QQC2.ToolTip.text: form.slotInfo.treeCommit
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    Kirigami.SelectableLabel {
        Kirigami.FormData.label: i18nc("@label", "Kernel:")
        visible: form.slotInfo.present
        text: form.slotInfo.kernel !== "" ? form.slotInfo.kernel : i18nc("@info", "None on the boot partition")
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 kernel", "%1 kernel: %2", form.slotInfo.title, text)
    }

    QQC2.Label {
        Kirigami.FormData.label: i18nc("@label disk encryption", "Encryption:")
        visible: form.slotInfo.present
        text: form.slotInfo.encrypted ? i18nc("@info", "Encrypted") : i18nc("@info", "Not encrypted")
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 encryption", "%1 encryption: %2", form.slotInfo.title, text)
    }

    QQC2.Button {
        id: startButton
        visible: form.slotInfo.present && !form.slotInfo.running && !form.slotInfo.trial
        enabled: form.slotInfo.bootable && !form.busy
        text: i18nc("@action:button", "Start This Version Next Time…")
        icon.name: "system-reboot"
        Accessible.name: i18nc("@action:button %1 slot title", "Start %1 next time", form.slotInfo.title)
        Accessible.description: form.slotInfo.bootable ? i18nc("@info:whatsthis", "The next restart starts this version once. It is kept only if it starts properly.") : form.slotInfo.reason
        onClicked: form.startRequested(form.slotInfo)
    }

    QQC2.Label {
        visible: startButton.visible && !form.slotInfo.bootable && form.slotInfo.reason !== ""
        Layout.fillWidth: true
        text: form.slotInfo.reason
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        Accessible.name: i18nc("@info:whatsthis %1 reason", "Why it cannot be started: %1", text)
    }
}

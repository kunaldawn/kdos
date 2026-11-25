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
 * starts it next time. The button exists only on a filled slot that is
 * neither running nor already armed, so the page never offers to start the
 * version the computer is already on, nor a root an update stopped writing
 * part way, whose version, build, commit and kernel rows would describe a
 * system that is no longer whole.
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

    /* Holds a whole system: present, and not left part written by an
     * update that stopped. */
    readonly property bool filled: slotInfo.present && slotInfo.state !== "incomplete"

    signal startRequested(var slotInfo)

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: form.slotInfo.title
    }

    /* A wrapped label's implicit width is its text on one line, and
     * FormLayout, twinned with the page's other forms, keeps its label:value
     * layout only while the widest implicit width fits: the state and the
     * reason are bounded to the width of the page's help text, or a long
     * sentence drops every form into the stacked layout and runs under the
     * scroll bar. */
    QQC2.Label {
        Kirigami.FormData.label: i18nc("@label slot state", "State:")
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        text: form.slotInfo.stateText
        wrapMode: Text.Wrap
        color: form.slotInfo.failed || (form.slotInfo.present && !form.filled) ? Kirigami.Theme.negativeTextColor
             : form.slotInfo.trial ? Kirigami.Theme.neutralTextColor
             : Kirigami.Theme.textColor
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 state", "%1 state: %2", form.slotInfo.title, text)
    }

    /* An incomplete slot has no version: what its record names is the
     * system the update began overwriting. */
    Kirigami.SelectableLabel {
        Kirigami.FormData.label: i18nc("@label", "Version:")
        visible: !form.slotInfo.present || form.filled
        text: form.slotInfo.versionText
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 version", "%1 version: %2", form.slotInfo.title, text)
    }

    Kirigami.SelectableLabel {
        Kirigami.FormData.label: i18nc("@label when the system was built", "Built:")
        visible: form.filled
        text: form.slotInfo.buildTimeText
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 date", "%1 built: %2", form.slotInfo.title, text)
    }

    Kirigami.SelectableLabel {
        id: commitLabel
        Kirigami.FormData.label: i18nc("@label the commit of the recipe tree the system was built from", "Tree commit:")
        visible: form.filled
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
        visible: form.filled
        text: form.slotInfo.kernel !== "" ? form.slotInfo.kernel : i18nc("@info", "None on the boot partition")
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 kernel", "%1 kernel: %2", form.slotInfo.title, text)
    }

    QQC2.Label {
        Kirigami.FormData.label: i18nc("@label disk encryption", "Encryption:")
        visible: form.filled
        text: form.slotInfo.encrypted ? i18nc("@info", "Encrypted") : i18nc("@info", "Not encrypted")
        Accessible.name: i18nc("@info:whatsthis %1 slot title, %2 encryption", "%1 encryption: %2", form.slotInfo.title, text)
    }

    QQC2.Button {
        id: startButton
        visible: form.filled && !form.slotInfo.running && !form.slotInfo.trial
        enabled: form.slotInfo.bootable && !form.busy
        text: i18nc("@action:button", "Start This Version Next Time…")
        icon.name: "system-reboot"
        Accessible.name: i18nc("@action:button %1 slot title", "Start %1 next time", form.slotInfo.title)
        Accessible.description: form.slotInfo.bootable ? i18nc("@info:whatsthis", "The next restart starts this version once. It is kept when a session runs for a minute, or, on a computer that logs nobody in automatically, when the login screen does.") : form.slotInfo.reason
        onClicked: form.startRequested(form.slotInfo)
    }

    QQC2.Label {
        objectName: "slotReason"
        visible: (startButton.visible || (form.slotInfo.present && !form.filled)) && !form.slotInfo.bootable && form.slotInfo.reason !== ""
        Layout.fillWidth: true
        Layout.preferredWidth: Kirigami.Units.gridUnit * 24
        Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        text: form.slotInfo.reason
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        Accessible.name: i18nc("@info:whatsthis %1 reason", "Why it cannot be started: %1", text)
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   DiskPage — which disk, and how it is laid out
 *
 * Guided: the whole disk erased and laid out by kinstall (the ESP, the BIOS
 * boot partition, and with the two system slots the A/B volume group).
 * Existing partitions: an ESP and a root chosen from what is there. Manual:
 * Partition Manager makes the partitions, and the page comes back to the
 * existing-partitions choice with the disks probed again.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Disk")

    readonly property var answers: Backend.answers
    readonly property var disks: Backend.probe.disks || []
    readonly property var lvs: Backend.probe.logical_volumes || []
    readonly property var disk: {
        for (let i = 0; i < disks.length; i++) {
            if (disks[i].path === answers.disk) {
                return disks[i];
            }
        }
        return null;
    }
    readonly property double needBytes: (Backend.probe.payload_kb || 0) * 1024

    function diskBytes(d) {
        return d.sectors * 512;
    }
    function usable(d) {
        return !d.boot_media && !d.readonly && diskBytes(d) >= needBytes;
    }
    function why(d) {
        if (d.boot_media) {
            return i18n("the medium this runs from");
        }
        if (d.readonly) {
            return i18n("read-only");
        }
        if (diskBytes(d) < needBytes) {
            return i18n("smaller than the %1 the install needs", Backend.formatSize(needBytes));
        }
        return "";
    }
    function partLabel(p) {
        return i18nc("@item:inlistbox %1 device %2 size %3 filesystem", "%1 — %2 %3",
                     p.path, Backend.formatSize(p.sectors * 512), p.fstype || i18nc("@item no filesystem", "empty"));
    }
    readonly property var parts: disk ? disk.partitions : []
    readonly property var roots: {
        const out = [];
        for (let i = 0; i < parts.length; i++) {
            if (!parts[i].esp && !parts[i].bios_boot) {
                out.push(parts[i]);
            }
        }
        for (let k = 0; k < lvs.length; k++) {
            out.push(lvs[k]);
        }
        return out;
    }

    complete: disk !== null && usable(disk)
              && (answers.plan === "wipe"
                  || (answers.esp.length > 0 && answers.root.length > 0 && answers.esp !== answers.root))
              && (answers.swap === "none" || answers.swapMb > 0)
              && !Backend.partitionManagerRunning
    problem: disk === null ? i18n("Choose the disk to install on.")
           : !usable(disk) ? i18n("%1 cannot take the install: %2.", disk.path, why(disk))
           : Backend.partitionManagerRunning ? i18n("Close Partition Manager to go on.")
           : answers.plan !== "wipe" && (answers.esp.length === 0 || answers.root.length === 0)
             ? i18n("Choose an EFI system partition and a root.")
           : answers.esp === answers.root ? i18n("The EFI system partition and the root must differ.") : ""

    onEntered: {
        if (disks.length === 0) {
            Backend.refreshProbe();
        }
    }

    QQC2.Frame {
        Layout.fillWidth: true
        padding: 0

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            Repeater {
                model: page.disks
                delegate: QQC2.RadioDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    checked: modelData.path === page.answers.disk
                    enabled: page.usable(modelData)
                    text: i18nc("@option:radio %1 model %2 size %3 device", "%1, %2 (%3)",
                                modelData.model || i18nc("@item a disk with no model name", "Disk"),
                                Backend.formatSize(page.diskBytes(modelData)), modelData.path)
                    QQC2.ToolTip.text: page.why(modelData)
                    QQC2.ToolTip.visible: hovered && !enabled
                    Accessible.name: enabled ? text : i18nc("@info:whatsthis %1 disk %2 reason", "%1, not offered: %2", text, page.why(modelData))
                    onToggled: {
                        page.answers.disk = modelData.path;
                        page.answers.esp = "";
                        page.answers.root = "";
                    }
                }
            }
        }
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true

        QQC2.RadioButton {
            Kirigami.FormData.label: i18nc("@label", "Layout:")
            text: i18nc("@option:radio", "Erase the disk and install KDOS")
            checked: page.answers.plan === "wipe"
            onToggled: page.answers.plan = "wipe"
        }
        QQC2.RadioButton {
            text: i18nc("@option:radio", "Use partitions that already exist")
            checked: page.answers.plan === "reuse"
            enabled: page.parts.length > 0
            onToggled: page.answers.plan = "reuse"
        }
        QQC2.Button {
            text: i18nc("@action:button", "Partition Manager…")
            icon.name: "partitionmanager"
            enabled: Backend.partitionManagerAvailable && !Backend.partitionManagerRunning
            QQC2.ToolTip.text: Backend.partitionManagerAvailable
                ? i18n("Make the partitions yourself; this page then uses them")
                : i18n("Partition Manager is not installed")
            QQC2.ToolTip.visible: hovered
            onClicked: {
                page.answers.plan = "reuse";
                Backend.openPartitionManager();
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            visible: page.answers.plan === "wipe"
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("@label", "Updates:")
            visible: page.answers.plan === "wipe"
            text: i18nc("@option:check", "Two system slots, so an update can be undone")
            checked: page.answers.lvm
            onToggled: page.answers.lvm = checked
        }
        QQC2.Label {
            visible: page.answers.plan === "wipe" && page.answers.lvm
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("An update is written into the slot not running and can be rolled back. Both slots share /home and the swap. A disk too small for two copies of the system gets one; the summary says which.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            visible: page.answers.plan === "reuse"
        }
        QQC2.ComboBox {
            id: espBox
            Kirigami.FormData.label: i18nc("@label:listbox", "EFI system partition:")
            visible: page.answers.plan === "reuse"
            Layout.fillWidth: true
            model: page.parts
            textRole: "path"
            displayText: currentIndex >= 0 ? page.partLabel(page.parts[currentIndex]) : i18nc("@item:inlistbox", "Choose…")
            currentIndex: {
                for (let i = 0; i < page.parts.length; i++) {
                    if (page.parts[i].path === page.answers.esp) {
                        return i;
                    }
                }
                return -1;
            }
            delegate: QQC2.ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: page.partLabel(modelData) + (modelData.esp ? " — " + i18nc("@item", "EFI system") : "")
            }
            Accessible.name: i18nc("@label:listbox", "EFI system partition")
            onActivated: index => page.answers.esp = page.parts[index].path
        }
        QQC2.CheckBox {
            visible: page.answers.plan === "reuse"
            text: i18nc("@option:check", "Format it (leave this off when another system boots from it)")
            checked: page.answers.formatEsp
            onToggled: page.answers.formatEsp = checked
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Root, formatted:")
            visible: page.answers.plan === "reuse"
            Layout.fillWidth: true
            model: page.roots
            textRole: "path"
            displayText: currentIndex >= 0 ? page.partLabel(page.roots[currentIndex]) : i18nc("@item:inlistbox", "Choose…")
            currentIndex: {
                for (let i = 0; i < page.roots.length; i++) {
                    if (page.roots[i].path === page.answers.root) {
                        return i;
                    }
                }
                return -1;
            }
            delegate: QQC2.ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: page.partLabel(modelData)
            }
            Accessible.name: i18nc("@label:listbox", "Root partition or volume, which is formatted")
            onActivated: index => page.answers.root = page.roots[index].path
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Filesystem:")
            model: ["ext4", "btrfs", "xfs", "f2fs"]
            currentIndex: Math.max(0, model.indexOf(page.answers.fstype))
            Accessible.name: i18nc("@label:listbox", "Filesystem")
            onActivated: index => page.answers.fstype = model[index]
        }
        QQC2.ComboBox {
            id: swapBox
            Kirigami.FormData.label: i18nc("@label:listbox", "Swap:")
            readonly property var values: ["none", "file", "partition"]
            model: [i18nc("@item:inlistbox swap", "None"),
                    i18nc("@item:inlistbox swap", "A file, or a volume with two slots"),
                    i18nc("@item:inlistbox swap", "A partition")]
            currentIndex: Math.max(0, values.indexOf(page.answers.swap))
            Accessible.name: i18nc("@label:listbox", "Swap")
            onActivated: index => page.answers.swap = values[index]
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: i18nc("@label:spinbox", "Swap size (MiB):")
            visible: page.answers.swap !== "none"
            from: 256
            to: 262144
            stepSize: 256
            editable: true
            value: page.answers.swapMb
            Accessible.name: i18nc("@label:spinbox", "Swap size in mebibytes")
            onValueModified: page.answers.swapMb = value
        }
    }
}

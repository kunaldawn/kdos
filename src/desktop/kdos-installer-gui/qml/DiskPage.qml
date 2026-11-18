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
 * Partition Manager makes the partitions; when it closes the disks are
 * probed again, and the page turns to the existing-partitions choice if the
 * chosen disk then holds partitions, and keeps erasing it if it holds none.
 *
 * Each disk's row names what it holds, so two disks of one size are told
 * apart before one is erased: its partitions, or for a disk with none the
 * signature on the whole device (an LVM physical volume and its group, a
 * filesystem), or "Empty". The rows take keyboard focus: Tab reaches them,
 * Space chooses one, and the focused row is framed in the focus colour
 * round both its lines. Choosing a disk with no partitions turns to the
 * erase plan, since the reuse plan has nothing on it to use.
 *
 * Look Again on the live medium probes as root, through the same pkexec
 * action as the install, which the live rule grants without a password:
 * only a root probe activates the LVM volume groups of a disk attached
 * after boot, whose volumes are otherwise missing from the root list.
 *
 * THE SWAP IS REFUSED HERE, where it is chosen: after each change of an
 * answer the page asks kinstall's plan (Backend.checkPlan), and its
 * `swap_problem`, the target check's swap rule alone, is shown under the
 * swap size and holds Next. The plan decides on the payload the last root
 * probe measured, which Backend hands on, so the largest swap it names is
 * the one the Summary's root plan accepts.
 *
 * WHETHER A DISK CAN TAKE THE INSTALL, AND HOLDS TWO SLOTS, IS kinstall's
 * ANSWER: `fits`, `problem` and `ab_fits` in its probe, by the rules the
 * install itself applies. The page keeps no size of its own. The lists offer
 * only what the reuse plan can take, and the Summary's refusal, which is
 * kinstall's whole target check, is the last word.
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
    /* What the copy takes: the least a reused root may hold. */
    readonly property double installBytes: Backend.probe.install_bytes || 0
    readonly property bool anyUsable: {
        for (let i = 0; i < disks.length; i++) {
            if (usable(disks[i])) {
                return true;
            }
        }
        return false;
    }
    /* The text column of a form: wrapped notes stay this wide, so their
     * one-line width never widens the form past the page. */
    readonly property real noteWidth: Kirigami.Units.gridUnit * 24

    function diskBytes(d) {
        return d.bytes !== undefined ? d.bytes : d.sectors * 512;
    }
    function usable(d) {
        return d.fits === true;
    }
    /* Two sizes as a person reads them, with as many decimals as keep
     * them apart: "holds 5.5 GiB; needs 5.5 GiB" reads as no reason. */
    function apart(have, need) {
        for (let p = 1; p < 3; p++) {
            const a = Backend.formatSize(have, p);
            const b = Backend.formatSize(need, p);
            if (a !== b) {
                return [a, b];
            }
        }
        return [Backend.formatSize(have, 3), Backend.formatSize(need, 3)];
    }
    /* kinstall's reason, except that a disk too small is said in this
     * page's units. */
    function why(d) {
        const need = Backend.probe.need_bytes || 0;
        if (!d.boot_media && !d.readonly && need > 0 && diskBytes(d) < need) {
            const s = apart(diskBytes(d), need);
            return i18nc("@info %1 device %2 disk size %3 the least the install needs", "%1 holds %2; the install needs at least %3", d.path, s[0], s[1]);
        }
        return d.problem || "";
    }
    /* kinstall's refusal as a sentence: its clauses start in lower case
     * to follow a front end's own words, and here they stand alone. */
    function sentence(t) {
        return t.length > 0 ? t.charAt(0).toUpperCase() + t.slice(1) + "." : t;
    }
    /* What a disk holds, by partition, for its row; a disk with none by
     * the signature on the whole device. */
    function holds(d) {
        const ps = d.partitions || [];
        if (ps.length === 0) {
            if (d.fstype === "LVM2_member") {
                return d.vg ? i18nc("@info %1 volume group", "LVM physical volume (volume group %1)", d.vg)
                            : i18nc("@info", "LVM physical volume");
            }
            if (d.fstype) {
                return i18nc("@info %1 filesystem type", "Holds: %1 on the whole disk", d.fstype);
            }
            return i18nc("@info a disk with no partitions", "Empty");
        }
        const out = [];
        for (let i = 0; i < ps.length; i++) {
            const p = ps[i];
            const kind = p.esp ? i18nc("@item a partition type", "EFI system")
                       : p.bios_boot ? i18nc("@item a partition type", "BIOS boot")
                       : p.fstype || i18nc("@item no filesystem", "empty");
            out.push(p.label ? i18nc("@item %1 type %2 label %3 size", "%1 “%2” %3", kind, p.label, Backend.formatSize(p.sectors * 512))
                             : i18nc("@item %1 type %2 size", "%1 %2", kind, Backend.formatSize(p.sectors * 512)));
        }
        return i18nc("@info %1 the partitions of a disk", "Holds: %1", out.join(", "));
    }
    function partLabel(p) {
        const kind = p.esp ? " — " + i18nc("@item a partition type", "EFI system")
                   : p.bios_boot ? " — " + i18nc("@item a partition type", "BIOS boot") : "";
        return i18nc("@item:inlistbox %1 device %2 size %3 filesystem", "%1 — %2 %3",
                     p.path, Backend.formatSize(p.sectors * 512), p.fstype || i18nc("@item no filesystem", "empty")) + kind;
    }
    readonly property var parts: disk ? disk.partitions : []
    /* An ESP is a partition typed as one or holding FAT; never the BIOS
     * boot partition, which holds Limine's raw stage. */
    readonly property var esps: {
        const out = [];
        for (let i = 0; i < parts.length; i++) {
            if (!parts[i].bios_boot && (parts[i].esp || parts[i].fstype === "vfat")) {
                out.push(parts[i]);
            }
        }
        return out;
    }
    /* A root is formatted: never an ESP, the BIOS boot partition or an LVM
     * physical volume, whose volumes are listed instead. */
    readonly property var roots: {
        const out = [];
        for (let i = 0; i < parts.length; i++) {
            if (!parts[i].esp && !parts[i].bios_boot && parts[i].fstype !== "LVM2_member") {
                out.push(parts[i]);
            }
        }
        for (let k = 0; k < lvs.length; k++) {
            out.push(lvs[k]);
        }
        return out;
    }
    function find(list, path) {
        for (let i = 0; i < list.length; i++) {
            if (list[i].path === path) {
                return list[i];
            }
        }
        return null;
    }
    readonly property var espPart: find(esps, answers.esp)
    readonly property var rootPart: find(roots, answers.root)
    readonly property string reuseProblem: {
        if (answers.plan === "wipe") {
            return "";
        }
        if (!espPart || !rootPart) {
            return i18n("Choose an EFI system partition and a root.");
        }
        if (espPart.path === rootPart.path) {
            return i18n("The EFI system partition and the root must differ.");
        }
        if (rootPart.sectors * 512 < installBytes) {
            const s = apart(rootPart.sectors * 512, installBytes);
            return i18n("%1 holds %2; the install needs %3.", rootPart.path, s[0], s[1]);
        }
        if (rootPart.held) {
            return i18n("%1 is in use by an open container or volume group.", rootPart.path);
        }
        if (!answers.formatEsp && espPart.fstype !== "vfat") {
            return i18n("%1 holds no FAT filesystem: format it, or choose another.", espPart.path);
        }
        return "";
    }

    /* kinstall's plan for these answers, when it is for them: the Summary's
     * dump, asked here as this user. */
    readonly property var plan: Backend.plan
    readonly property bool planFresh: plan.disk === answers.disk && plan.plan === answers.plan
                                      && plan.swap === answers.swap
                                      && plan.swap_mb === (answers.swap === "none" ? 0 : answers.swapMb)
                                      && (plan.lvm !== null && plan.lvm !== undefined) === answers.lvm
    readonly property string swapProblem: answers.plan === "wipe" && planFresh ? (plan.swap_problem || "") : ""
    /* The erase plan's swap is checked only by that plan: Next waits for
     * the one made for these answers, or a swap that does not fit passes
     * this page in the pause before it arrives. */
    readonly property bool swapChecking: answers.plan === "wipe" && answers.swap !== "none" && !planFresh

    complete: disk !== null && usable(disk)
              && reuseProblem.length === 0
              && (answers.swap === "none" || answers.swapMb > 0)
              && !swapChecking
              && swapProblem.length === 0
              && !Backend.partitionManagerRunning
    problem: Backend.busy && disks.length === 0 ? i18n("Looking at this computer's disks…")
           : !anyUsable ? i18n("No disk here can take the install; each one says why.")
           : disk === null ? i18n("Choose the disk to install on.")
           : !usable(disk) ? i18n("%1 cannot take the install: %2.", disk.path, why(disk))
           : Backend.partitionManagerRunning ? i18n("Close Partition Manager to go on.")
           : swapProblem.length > 0 ? sentence(swapProblem)
           : swapChecking ? i18n("Checking that the swap fits…")
           : reuseProblem

    /* Partition Manager ran and has closed, and the probe that answers
     * next decides the plan. One that failed to start sets nothing. */
    property bool partitionManagerUsed: false

    function useReuse() {
        answers.plan = "reuse";
        /* The reuse plan makes no partition, so a swap partition would be
         * no swap at all. */
        if (answers.swap === "partition") {
            answers.swap = "file";
        }
    }

    /* Every visit asks again, so a disk attached since the last look is
     * listed. */
    onEntered: {
        Backend.refreshProbe();
        planTimer.restart();
    }

    /* The plan after a pause, so a spin box's steps make one dump; only
     * while this page is shown and the erase plan, the one with a swap
     * rule, is chosen on a disk that can take the install. */
    Timer {
        id: planTimer
        interval: 400
        onTriggered: {
            if (page.visible && page.answers.plan === "wipe" && page.disk !== null && page.usable(page.disk)) {
                Backend.checkPlan();
            }
        }
    }
    Connections {
        target: page.answers
        function onChanged() {
            if (page.visible) {
                planTimer.restart();
            }
        }
    }

    Connections {
        target: Backend
        function onPartitionManagerClosed() {
            page.partitionManagerUsed = true;
        }
        function onProbeChanged() {
            if (page.visible) {
                planTimer.restart();
            }
            if (!page.partitionManagerUsed || Backend.partitionManagerRunning) {
                return;
            }
            page.partitionManagerUsed = false;
            const ds = Backend.probe.disks || [];
            for (let i = 0; i < ds.length; i++) {
                if (ds[i].path === page.answers.disk) {
                    if ((ds[i].partitions || []).length > 0) {
                        page.useReuse();
                    } else {
                        page.answers.plan = "wipe";
                    }
                    return;
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("The disk to install on:")
        }
        QQC2.ToolButton {
            text: i18nc("@action:button probe the disks again", "Look Again")
            icon.name: "view-refresh"
            enabled: !Backend.busy && !Backend.partitionManagerRunning
            QQC2.ToolTip.text: i18n("Look for disks again, after attaching one")
            QQC2.ToolTip.visible: hovered
            onClicked: Backend.lookAgain()
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
                delegate: Item {
                    id: row
                    required property var modelData
                    Layout.fillWidth: true
                    implicitWidth: rowLines.implicitWidth
                    implicitHeight: rowLines.implicitHeight

                    /* The keyboard's place, framed round both lines of the
                     * row: the delegate's own highlight covers only the
                     * first, and on a dark ground it is barely a shade. */
                    Rectangle {
                        anchors.fill: parent
                        z: 1
                        visible: diskRadio.visualFocus
                        color: "transparent"
                        border.color: Kirigami.Theme.focusColor
                        border.width: 2
                        radius: Kirigami.Units.cornerRadius
                    }

                    ColumnLayout {
                        id: rowLines
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 0

                        /* A delegate takes no focus by default, which leaves a
                         * keyboard no way to choose the disk. */
                        QQC2.RadioDelegate {
                            id: diskRadio
                            Layout.fillWidth: true
                            focusPolicy: Qt.StrongFocus
                            checked: row.modelData.path === page.answers.disk
                            enabled: page.usable(row.modelData)
                            text: i18nc("@option:radio %1 model %2 size %3 device", "%1, %2 (%3)",
                                        row.modelData.model || i18nc("@item a disk with no model name", "Disk"),
                                        Backend.formatSize(page.diskBytes(row.modelData)), row.modelData.path)
                            Accessible.name: enabled ? text : i18nc("@info:whatsthis %1 disk %2 reason", "%1, not offered: %2", text, page.why(row.modelData))
                            Accessible.description: page.holds(row.modelData)
                            onToggled: {
                                page.answers.disk = row.modelData.path;
                                page.answers.esp = "";
                                page.answers.root = "";
                                page.answers.plan = Backend.planForDisk(page.answers.plan, (row.modelData.partitions || []).length);
                            }
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            Layout.rightMargin: Kirigami.Units.largeSpacing
                            Layout.bottomMargin: page.usable(row.modelData) ? Kirigami.Units.smallSpacing : 0
                            wrapMode: Text.Wrap
                            color: Kirigami.Theme.disabledTextColor
                            text: page.holds(row.modelData)
                            Accessible.ignored: true
                        }
                        /* Why a disk is not offered, on its row: a tooltip shows
                         * only to a pointer resting on it. */
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            Layout.rightMargin: Kirigami.Units.largeSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                            visible: !page.usable(row.modelData)
                            wrapMode: Text.Wrap
                            color: Kirigami.Theme.neutralTextColor
                            text: page.why(row.modelData)
                            Accessible.ignored: true
                        }
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
            onToggled: page.useReuse()
        }
        QQC2.Button {
            text: i18nc("@action:button", "Partition Manager…")
            icon.name: "partitionmanager"
            enabled: Backend.partitionManagerAvailable && !Backend.partitionManagerRunning
            QQC2.ToolTip.text: Backend.partitionManagerAvailable
                ? i18n("Make the partitions yourself; this page then uses them")
                : i18n("Partition Manager is not installed")
            QQC2.ToolTip.visible: hovered
            onClicked: Backend.openPartitionManager()
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
            visible: page.answers.plan === "wipe" && page.answers.lvm && page.disk !== null && page.disk.ab_fits === true
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            wrapMode: Text.Wrap
            text: i18n("An update is written into the slot not running and can be rolled back. Both slots share /home and the swap.")
        }
        QQC2.Label {
            visible: page.answers.plan === "wipe" && page.answers.lvm && page.disk !== null && page.disk.ab_fits === true
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            wrapMode: Text.Wrap
            text: i18n("This disk holds two slots.")
        }
        Kirigami.InlineMessage {
            visible: page.answers.plan === "wipe" && page.disk !== null && page.disk.fstype === "LVM2_member"
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            type: Kirigami.MessageType.Warning
            text: page.disk && page.disk.vg
                ? i18n("This disk is an LVM physical volume of the volume group %1; erasing it destroys that group's volumes.", page.disk.vg)
                : i18n("This disk is an LVM physical volume; erasing it destroys the volumes of its group.")
        }
        Kirigami.InlineMessage {
            visible: page.answers.plan === "wipe" && page.answers.lvm && page.disk !== null && page.disk.ab_fits !== true
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            type: Kirigami.MessageType.Warning
            text: i18n("Two slots need %1; this disk gets one, and an update cannot be undone. Left ticked, the box still puts the system on an LVM volume group, so a second slot can be added later.",
                       Backend.formatSize(Backend.probe.ab_need_bytes || 0))
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
            model: page.esps
            textRole: "path"
            displayText: currentIndex >= 0 ? page.partLabel(page.esps[currentIndex]) : i18nc("@item:inlistbox", "Choose…")
            currentIndex: {
                for (let i = 0; i < page.esps.length; i++) {
                    if (page.esps[i].path === page.answers.esp) {
                        return i;
                    }
                }
                return -1;
            }
            /* ComboBox also makes a delegate outside its popup's list, to
             * measure it, where ListView.view is null. The popup is as wide
             * as the field, so a long partition line is not cut. */
            popup.width: width
            delegate: QQC2.ItemDelegate {
                required property var modelData
                width: ListView.view ? ListView.view.width : implicitWidth
                text: page.partLabel(modelData)
            }
            Accessible.name: i18nc("@label:listbox", "EFI system partition")
            onActivated: index => page.answers.esp = page.esps[index].path
        }
        QQC2.CheckBox {
            visible: page.answers.plan === "reuse"
            text: i18nc("@option:check", "Format it (leave this off when another system boots from it)")
            checked: page.answers.formatEsp
            onToggled: page.answers.formatEsp = checked
        }
        QQC2.ComboBox {
            id: rootBox
            Kirigami.FormData.label: i18nc("@label:listbox", "Root, formatted:")
            visible: page.answers.plan === "reuse"
            Layout.fillWidth: true
            model: page.roots
            textRole: "path"
            displayText: currentIndex >= 0 ? page.partLabel(page.roots[currentIndex])
                       : page.roots.length === 0 ? i18nc("@item:inlistbox", "No partition or volume to use")
                       : i18nc("@item:inlistbox", "Choose…")
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
                width: ListView.view ? ListView.view.width : implicitWidth
                text: page.partLabel(modelData)
            }
            popup.width: width
            Accessible.name: i18nc("@label:listbox", "Root partition or volume, which is formatted")
            onActivated: index => page.answers.root = page.roots[index].path
        }
        QQC2.Label {
            visible: page.answers.plan === "reuse" && page.roots.length === 0
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            wrapMode: Text.Wrap
            text: !page.parts.some(p => p.fstype === "LVM2_member")
                ? i18n("This disk has no partition a root can go on: make one with Partition Manager.")
                : Backend.probe.live === true
                ? i18n("This disk's LVM volumes are not active yet: Look Again activates them.")
                : i18n("This disk's LVM volumes are not active: activate them as root with “vgchange -aay”, then Look Again.")
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
        /* A swap partition is made by the erase plan alone, so the reuse
         * plan has a list of its own with none or a file. Two lists rather
         * than one whose model changes: a ComboBox given a new model resets
         * its index and drops the binding to the answer. */
        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Swap:")
            visible: page.answers.plan !== "reuse"
            readonly property var values: ["none", "file", "partition"]
            model: [i18nc("@item:inlistbox swap", "None"),
                    i18nc("@item:inlistbox swap", "A file, or a volume with two slots"),
                    i18nc("@item:inlistbox swap", "A partition")]
            currentIndex: Math.max(0, values.indexOf(page.answers.swap))
            Accessible.name: i18nc("@label:listbox", "Swap")
            onActivated: index => page.answers.swap = values[index]
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Swap:")
            visible: page.answers.plan === "reuse"
            readonly property var values: ["none", "file"]
            model: [i18nc("@item:inlistbox swap", "None"), i18nc("@item:inlistbox swap", "A file")]
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
        Kirigami.InlineMessage {
            visible: page.swapProblem.length > 0
            Layout.fillWidth: true
            Layout.maximumWidth: page.noteWidth
            type: Kirigami.MessageType.Error
            text: page.sentence(page.swapProblem)
        }
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   SummaryPage — what kinstall says these answers will do
 *
 * Read from `kinstall --dump plan --json` over the same answer file the
 * install is given, so what is shown is what will run, skipped steps
 * included. Answers the backend refuses are shown as its refusal, and the
 * install cannot be started from here until they are changed. A disk that
 * gets one system slot where two were asked for is a warning above the
 * list, and KDOS application groups are named by their titles.
 *
 * Kirigami.FormLayout puts the labels beside the values only while its
 * widest value fits beside its label, and otherwise stacks every row label
 * above value. Each value is a Value: it fills its column, wraps, and asks
 * for at most 34 grid units, so one long value wraps in its column and the
 * form keeps two columns at the installer's default window.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Summary")
    readonly property var plan: Backend.plan
    readonly property var answers: Backend.answers
    readonly property bool fresh: plan.disk !== undefined && plan.disk === answers.disk
    readonly property string refused: plan.refused || ""

    complete: fresh && refused.length === 0 && !Backend.busy
    problem: Backend.busy || !fresh ? i18n("Asking the installer what it will do…") : refused

    onEntered: Backend.refreshPlan()

    component Value: QQC2.Label {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.min(implicitWidth, Kirigami.Units.gridUnit * 34)
        wrapMode: Text.Wrap
    }

    /* A native group by its title on the Applications page; the id where
     * the page has none. */
    function groupTitle(id) {
        const native = Backend.apps.native || [];
        for (let i = 0; i < native.length; i++) {
            if (native[i].id === id) {
                return native[i].title || id;
            }
        }
        return id;
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        visible: page.fresh && page.refused.length > 0
        text: i18n("The installer refuses these answers: %1", page.refused)
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: page.fresh && page.refused.length === 0
        text: page.plan.plan === "wipe"
            ? i18n("Everything on %1 is erased when the install starts.", page.plan.disk)
            : i18n("%1 is formatted when the install starts; the other partitions are kept.", page.plan.root)
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: page.fresh && page.refused.length === 0 && !page.plan.ab && !!page.plan.lvm
        text: i18n("This disk has no room for a second system slot, so an update cannot be undone.")
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true
        visible: page.fresh

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Disk")
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Disk:")
            text: page.plan.disk || ""
            Accessible.name: i18nc("@info:whatsthis", "Disk: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "System:")
            text: page.plan.ab
                ? i18n("Two system slots of %1 each, so an update can be undone; /home and the swap shared",
                       Backend.formatSize((page.plan.lvm ? page.plan.lvm.slot_mb : 0) * 1048576))
                : page.plan.lvm ? i18n("One system slot")
                : i18n("One system on %1", page.plan.root || "")
            Accessible.name: i18nc("@info:whatsthis", "System: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Filesystem:")
            text: page.plan.fstype || ""
            Accessible.name: i18nc("@info:whatsthis", "Filesystem: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Encryption:")
            text: page.plan.luks ? i18n("LUKS2, a passphrase at every start") : i18n("None")
            Accessible.name: i18nc("@info:whatsthis", "Encryption: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Swap:")
            /* As a string, so the number reads as kinstall's refusal
             * writes it ("4096 MiB"), with no digit grouping. */
            text: page.plan.swap === "none" ? i18n("None") : i18n("%1 MiB", String(page.plan.swap_mb))
            Accessible.name: i18nc("@info:whatsthis", "Swap: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Starts on:")
            text: page.plan.bios_boot === null ? i18n("UEFI only") : i18n("UEFI and legacy BIOS")
            Accessible.name: i18nc("@info:whatsthis", "Starts on: %1", text)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "You")
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Account:")
            text: page.plan.oem ? i18n("None — the first start runs the setup wizard")
                : i18n("%1 (%2)%3", page.plan.fullname || "", page.plan.username || "",
                       page.plan.wheel ? i18n(", administrator") : "")
            Accessible.name: i18nc("@info:whatsthis", "Account: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Login:")
            text: page.plan.oem ? i18n("The setup wizard")
                : !page.plan.autologin ? i18n("The login screen")
                : page.plan.autologin_lock ? i18n("Automatic, into a locked session")
                : i18n("Automatic, no password after the disk's")
            Accessible.name: i18nc("@info:whatsthis", "Login: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Root:")
            text: page.plan.root_locked ? i18n("Locked") : i18n("A password is set")
            Accessible.name: i18nc("@info:whatsthis", "Root: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Computer's name:")
            text: page.plan.hostname || ""
            Accessible.name: i18nc("@info:whatsthis", "Computer's name: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Keyboard:")
            text: Backend.keyboards.description(page.plan.xkb_layout || "us")
                  + (page.plan.xkb_variant
                     ? " — " + Backend.keyboards.variantDescription(page.plan.xkb_layout || "us", page.plan.xkb_variant) : "")
            Accessible.name: i18nc("@info:whatsthis", "Keyboard: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Time zone:")
            text: page.plan.timezone_label || ""
            Accessible.name: i18nc("@info:whatsthis", "Time zone: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Look:")
            text: i18nc("@info %1 accent %2 desk", "%1 accent, %2 desk",
                        Backend.accentLabel(page.plan.theme || ""), Backend.deskTitle(page.plan.desk || ""))
            Accessible.name: i18nc("@info:whatsthis", "Look: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Applications:")
            text: !page.plan.apps || page.plan.apps.length === 0 ? i18n("None")
                /* One sentence in one i18np, so the pronoun agrees with
                 * the count in every language. */
                : i18np("%1 group, about %2 — recorded; the first login offers to build its applications",
                        "%1 groups, about %2 — recorded; the first login offers to build their applications",
                        page.plan.apps.length, Backend.formatSize(page.plan.apps_bytes))
            Accessible.name: i18nc("@info:whatsthis", "Applications: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "KDOS applications:")
            text: !page.plan.groups || page.plan.groups.length === 0 ? i18n("None")
                : page.plan.groups.map(g => page.groupTitle(g)).join(", ") + " — "
                  /* With no binary host named the install only records
                   * them; the Done page's warning says the same. The
                   * command stands on a line of its own, so a long list of
                   * groups never wraps it at a space. */
                  + (page.plan.binhost ? i18n("downloaded during the install")
                     : i18np("recorded; this installs it once a binary host answers:\nsudo kdos update install --pending",
                             "recorded; this installs them once a binary host answers:\nsudo kdos update install --pending",
                             page.plan.groups.length))
            Accessible.name: i18nc("@info:whatsthis", "KDOS applications: %1", text)
        }
        Value {
            Kirigami.FormData.label: i18nc("@label", "Box images:")
            text: page.plan.appbox ? i18n("Kept from this live session") : i18n("Not copied")
            Accessible.name: i18nc("@info:whatsthis", "Box images: %1", text)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Steps")
        }
        Repeater {
            model: page.plan.steps || []
            delegate: Value {
                required property var modelData
                Kirigami.FormData.label: modelData.index + "."
                text: modelData.state === "skipped"
                    ? i18nc("@info %1 step", "%1 — skipped", modelData.title)
                    : i18nc("@info %1 step %2 what it does", "%1 — %2", modelData.title, modelData.detail)
                color: modelData.state === "skipped" ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                Accessible.name: text
            }
        }
    }

    QQC2.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: !page.fresh
        visible: running
        Accessible.name: i18nc("@info:whatsthis", "Asking the installer what it will do")
    }
}

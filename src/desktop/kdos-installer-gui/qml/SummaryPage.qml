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
 * install cannot be started from here until they are changed.
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

    Kirigami.FormLayout {
        Layout.fillWidth: true
        visible: page.fresh

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Disk")
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Disk:")
            text: page.plan.disk || ""
            Accessible.name: i18nc("@info:whatsthis", "Disk: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "System:")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: page.plan.ab
                ? i18n("Two system slots of %1 each, so an update can be undone; /home and the swap shared",
                       Backend.formatSize((page.plan.lvm ? page.plan.lvm.slot_mb : 0) * 1048576))
                : page.plan.lvm ? i18n("One system slot: this disk has no room for a second, so updates cannot be undone")
                : i18n("One system on %1", page.plan.root || "")
            color: page.plan.ab ? Kirigami.Theme.textColor : Kirigami.Theme.neutralTextColor
            Accessible.name: i18nc("@info:whatsthis", "System: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Filesystem:")
            text: page.plan.fstype || ""
            Accessible.name: i18nc("@info:whatsthis", "Filesystem: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Encryption:")
            text: page.plan.luks ? i18n("LUKS2, a passphrase at every start") : i18n("None")
            Accessible.name: i18nc("@info:whatsthis", "Encryption: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Swap:")
            text: page.plan.swap === "none" ? i18n("None") : i18n("%1 MiB", page.plan.swap_mb)
            Accessible.name: i18nc("@info:whatsthis", "Swap: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Starts on:")
            text: page.plan.bios_boot === null ? i18n("UEFI only") : i18n("UEFI and legacy BIOS")
            Accessible.name: i18nc("@info:whatsthis", "Starts on: %1", text)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "You")
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Account:")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: page.plan.oem ? i18n("None — the first start runs the setup wizard")
                : i18n("%1 (%2)%3", page.plan.fullname || "", page.plan.username || "",
                       page.plan.wheel ? i18n(", administrator") : "")
            Accessible.name: i18nc("@info:whatsthis", "Account: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Login:")
            text: page.plan.oem ? i18n("The setup wizard")
                : !page.plan.autologin ? i18n("The login screen")
                : page.plan.autologin_lock ? i18n("Automatic, into a locked session")
                : i18n("Automatic, no password after the disk's")
            Accessible.name: i18nc("@info:whatsthis", "Login: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Computer's name:")
            text: page.plan.hostname || ""
            Accessible.name: i18nc("@info:whatsthis", "Computer's name: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Keyboard:")
            text: Backend.keyboards.description(page.plan.xkb_layout || "us")
                  + (page.plan.xkb_variant ? " (" + page.plan.xkb_variant + ")" : "")
            Accessible.name: i18nc("@info:whatsthis", "Keyboard: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Time zone:")
            text: page.plan.timezone_label || ""
            Accessible.name: i18nc("@info:whatsthis", "Time zone: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Look:")
            text: i18nc("@info %1 accent %2 desk", "%1 accent, %2 desk", page.plan.theme || "", page.plan.desk || "")
            Accessible.name: i18nc("@info:whatsthis", "Look: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Applications:")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: !page.plan.apps || page.plan.apps.length === 0 ? i18n("None")
                : i18np("%1 group, about %2", "%1 groups, about %2", page.plan.apps.length, Backend.formatSize(page.plan.apps_bytes))
                  + " — " + (page.plan.apps_route === "network" ? i18n("built during the install")
                           : page.plan.apps_route === "import" ? i18n("from the stick")
                           : i18n("offered at the first login"))
            Accessible.name: i18nc("@info:whatsthis", "Applications: %1", text)
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Steps")
        }
        Repeater {
            model: page.plan.steps || []
            delegate: QQC2.Label {
                required property var modelData
                Kirigami.FormData.label: modelData.index + "."
                text: modelData.state === "skipped"
                    ? i18nc("@info %1 step", "%1 — skipped", modelData.title)
                    : i18nc("@info %1 step %2 what it does", "%1 — %2", modelData.title, modelData.detail)
                color: modelData.state === "skipped" ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
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

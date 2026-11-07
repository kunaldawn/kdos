/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   InstallPage — the install, as kinstall's event stream tells it
 *
 * Entering the page starts it; there is no Back from here. The steps are the
 * plan kinstall sent first, and each moves as its events arrive.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Installing KDOS")
    readonly property var events: Backend.events
    complete: false

    onEntered: Backend.startInstall()

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: page.events.currentTitle.length > 0
            ? i18nc("@info %1 step", "%1…", page.events.currentTitle)
            : i18n("Waiting for permission to install…")
    }

    QQC2.ProgressBar {
        Layout.fillWidth: true
        from: 0
        to: 1
        value: page.events.overall
        indeterminate: page.events.currentStep < 0
        Accessible.name: i18nc("@info:whatsthis", "Install progress")
    }

    QQC2.Label {
        Layout.fillWidth: true
        text: page.events.note
        elide: Text.ElideRight
        color: Kirigami.Theme.disabledTextColor
    }

    Repeater {
        model: page.events
        delegate: RowLayout {
            id: stepRow
            required property string title
            required property string detail
            required property string stepState
            required property real frac
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                source: stepRow.stepState === "done" ? "checkmark"
                      : stepRow.stepState === "running" ? "media-playback-start"
                      : stepRow.stepState === "failed" ? "dialog-error"
                      : stepRow.stepState === "skipped" ? "go-next-skip"
                      : "content-loading-symbolic"
                opacity: stepRow.stepState === "pending" || stepRow.stepState === "skipped" ? 0.4 : 1
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: stepRow.stepState === "skipped"
                    ? i18nc("@info %1 step", "%1 — skipped", stepRow.title) : stepRow.title
                font.bold: stepRow.stepState === "running"
                color: stepRow.stepState === "failed" ? Kirigami.Theme.negativeTextColor
                     : stepRow.stepState === "pending" || stepRow.stepState === "skipped" ? Kirigami.Theme.disabledTextColor
                     : Kirigami.Theme.textColor
                Accessible.name: i18nc("@info:whatsthis %1 step %2 state", "%1: %2", stepRow.title, stepRow.stepState)
            }
            QQC2.Label {
                visible: stepRow.stepState === "running" && stepRow.frac >= 0
                text: i18nc("@info percent", "%1%", Math.round(stepRow.frac * 100))
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: page.events.warnings.length > 0
        text: page.events.warnings.join("\n")
    }
}

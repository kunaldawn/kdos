/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   DonePage — how it ended, and what next
 *
 * A finished install offers the restart; the warnings, things the installed
 * system will lack though it installed, are listed with it as WarningList
 * sets them out, the same list the Install page shows. A failed one
 * says why in kinstall's words, shows the last of the log where kinstall
 * sent any, and goes back to the summary, where the answers can be changed
 * and the install run again. The log file is named, and offered, only when
 * kinstall ran: an install refused permission never wrote one.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    signal retry()

    readonly property var events: Backend.events
    readonly property bool ok: events.status === 0
    heading: ok ? i18nc("@title", "KDOS is installed") : i18nc("@title", "The install did not finish")
    complete: false

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        icon.name: page.ok ? "data-success" : "data-error"
        text: page.ok ? i18n("Restart into KDOS, and take the install medium out when the screen goes dark.") : page.events.failure
        explanation: page.ok
            ? (Backend.answers.oem
               ? i18n("The first start runs the setup wizard, which makes the account.")
               : Backend.answers.autologin && Backend.answers.luks
                 ? (Backend.answers.autologinLock
                    ? i18n("After the disk passphrase, the session starts locked; your password unlocks it.")
                    : i18n("After the disk passphrase, the session starts by itself."))
                 : i18n("Log in with the account you made."))
            : page.events.started ? i18n("The full log is %1 on this live system.", Backend.logPath) : ""
    }

    WarningList {
        Layout.fillWidth: true
        source: page.events.warnings
        lead: i18n("Installed, with these exceptions:")
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Kirigami.Units.largeSpacing

        QQC2.Button {
            visible: page.ok
            text: i18nc("@action:button", "Restart Now")
            icon.name: "system-reboot"
            onClicked: Backend.reboot()
        }
        QQC2.Button {
            visible: !page.ok
            text: i18nc("@action:button", "Back to the Summary")
            icon.name: "go-previous"
            onClicked: page.retry()
        }
        QQC2.Button {
            visible: page.events.started && Backend.logReadable()
            text: i18nc("@action:button", "Open the Log")
            icon.name: "text-x-log"
            onClicked: Backend.openLog()
        }
        QQC2.Button {
            text: i18nc("@action:button", "Close")
            icon.name: "window-close"
            onClicked: Qt.quit()
        }
    }

    QQC2.Label {
        visible: !page.ok && page.events.log.length > 0
        text: i18nc("@title:group", "The end of the log")
        font.bold: true
    }
    QQC2.TextArea {
        visible: !page.ok && page.events.log.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 12
        readOnly: true
        wrapMode: TextEdit.Wrap
        font: Kirigami.Theme.fixedWidthFont
        text: page.events.log.slice(-40).join("\n")
        Accessible.name: i18nc("@label", "The end of the install log")
    }
}

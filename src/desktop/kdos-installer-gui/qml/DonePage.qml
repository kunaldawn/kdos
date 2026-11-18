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
 * system will lack though it installed, are listed with it, one bullet each,
 * a warning sent twice listed once, each starting with a capital, and the
 * commands in them set as code. A failed one
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

    /* A warning as rich text: escaped, and each `command` span set as code
     * that never breaks, its hyphens and spaces the non-breaking kind, so
     * `--pending` is not split across two lines. */
    function rich(text) {
        const esc = text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        return esc.replace(/`([^`]*)`/g, (m, code) =>
            "<code>" + code.replace(/-/g, "\u2011").replace(/ /g, "&nbsp;") + "</code>");
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        icon.name: page.ok ? "data-success" : "data-error"
        text: page.ok ? i18n("Remove the install medium, then restart into KDOS.") : page.events.failure
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

    /* kinstall's warnings, once each, as sentences: each is a clause that
     * starts in lower case, to follow whatever a front end puts before it. */
    readonly property var warnings: {
        const out = [];
        const ws = events.warnings;
        for (let i = 0; i < ws.length; i++) {
            const w = ws[i].length > 0 ? ws[i].charAt(0).toUpperCase() + ws[i].slice(1) : ws[i];
            if (w.length > 0 && out.indexOf(w) < 0) {
                out.push(w);
            }
        }
        return out;
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: page.warnings.length > 0
        text: "<p>" + i18n("Installed, with these exceptions:") + "</p><ul>"
              + page.warnings.map(w => "<li>" + page.rich(w) + "</li>").join("") + "</ul>"
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

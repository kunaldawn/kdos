/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   EncryptionPage — LUKS2 under the system, and its passphrase
 *
 * One passphrase at every boot opens all of it: with the two system slots the
 * whole volume group is inside the one container. There is no recovery key,
 * which the page says before anyone types one.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Encryption")
    readonly property var answers: Backend.answers

    complete: !answers.luks
              || (pass.text.length > 0 && pass.text === again.text && page.answers.fitsAnswerFile(pass.text))
    problem: !answers.luks ? ""
           : pass.text.length === 0 ? i18n("Type a passphrase, or turn encryption off.")
           : pass.text !== again.text ? i18n("The two passphrases do not match.")
           : i18n("A passphrase cannot start or end with a space here.")

    QQC2.Switch {
        text: i18nc("@option:check", "Encrypt the system disk")
        checked: page.answers.luks
        onToggled: {
            page.answers.luks = checked;
            if (!checked) {
                page.answers.autologin = false;
            }
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: i18n("Everything KDOS writes — the system, your files and the swap — is kept in a LUKS2 container, opened with a passphrase at every start. A lost or stolen disk then gives nothing away.")
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: page.answers.luks
        text: i18n("There is no recovery key. Without the passphrase, nothing on this disk can be read again — not by you, not by anyone.")
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Information
        visible: page.answers.luks && page.answers.plan === "reuse"
        text: i18n("The root partition you chose is encrypted, which erases what is on it; the other partitions are not touched.")
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true
        visible: page.answers.luks

        Kirigami.PasswordField {
            id: pass
            Kirigami.FormData.label: i18nc("@label:textbox", "Passphrase:")
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Disk passphrase")
            onTextChanged: page.answers.luksPassphrase = text
        }
        Kirigami.PasswordField {
            id: again
            Kirigami.FormData.label: i18nc("@label:textbox", "Again:")
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Disk passphrase, again")
        }
        QQC2.Label {
            text: pass.text.length === 0 ? ""
                : pass.text.length < 8 ? i18nc("@info passphrase strength", "Short: a passphrase of several words is far harder to guess")
                : pass.text.length < 16 ? i18nc("@info passphrase strength", "Fair")
                : i18nc("@info passphrase strength", "Long, good")
            color: pass.text.length < 8 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor
            Accessible.name: i18nc("@info:whatsthis", "Passphrase strength: %1", text)
        }
    }
}

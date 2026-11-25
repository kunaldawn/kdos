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
 *
 * The boot prompt types with the console keymap nearest the Keyboard page's
 * layout (EFI/kdos/keymap, loaded by the initramfs), which the page says
 * too: a character that console map has no key for can never be typed
 * there, and a layout kbd has no console map for leaves the US map.
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
           : again.text.length === 0 ? i18n("Type the passphrase again.")
           : pass.text !== again.text ? i18n("The two passphrases do not match.")
           : i18n("A passphrase cannot start or end with a space here.")

    /* A check box, not a switch: Breeze draws a switch's track the same
     * on and off but for a faint tint, and this choice must read at a
     * glance. */
    QQC2.CheckBox {
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
        visible: page.answers.luks
        text: i18n("At every start the passphrase is typed with the text console's keymap nearest the layout chosen on the Keyboard page, so a character that keymap has no key for cannot be entered. A layout with no console keymap of its own, such as Arabic or the Asian scripts, types with the US map there.")
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
            placeholderText: i18nc("@info:placeholder", "Passphrase")
            Accessible.name: i18nc("@label:textbox", "Disk passphrase")
            /* Tab goes to the second field, past this field's reveal
             * button, so what is typed after one Tab is the confirmation;
             * Shift+Tab from the second field reaches the reveal button
             * (the second field's backtab handler). */
            KeyNavigation.tab: again
            onTextChanged: page.answers.luksPassphrase = text
        }
        StrengthLabel {
            visible: page.answers.luks
            secret: pass.text
            field: pass
            available: page.availableWidth
        }
        Kirigami.PasswordField {
            id: again
            Kirigami.FormData.label: i18nc("@label:textbox", "Again:")
            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder", "Passphrase, again")
            Accessible.name: i18nc("@label:textbox", "Disk passphrase, again")
            /* Shift+Tab goes to the first field's reveal button, which Tab
             * from the first field passes over; the backtab the first
             * field's KeyNavigation.tab sets here lands in the field itself.
             * This handler takes the key first, KeyNavigation coming after
             * the field's own handling, and the first field is the stop
             * when it has no reveal button. */
            KeyNavigation.backtab: pass
            KeyNavigation.priority: KeyNavigation.AfterItem
            Keys.onBacktabPressed: event => {
                const reveal = pass.nextItemInFocusChain(true);
                let inside = false;
                for (let up = reveal; up && !inside; up = up.parent) {
                    inside = up === pass;
                }
                (inside ? reveal : pass).forceActiveFocus(Qt.BacktabFocusReason);
                event.accepted = true;
            }
        }
    }
}

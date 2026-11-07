/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   AccountPage — set up now, or leave it to the first person (OEM)
 *
 * "Set up now" makes the account here; it is the default for a machine a
 * person installs for themselves. OEM makes none: the first start runs the
 * setup wizard, which asks for the account itself. Automatic login is offered
 * only on an encrypted disk, where the passphrase at every start already
 * asks who is at the machine; anywhere else the login screen is that
 * question and is never skipped.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Account")
    readonly property var answers: Backend.answers
    property bool userEdited: false

    readonly property string accountProblem: {
        if (!answers.hostnameOk(answers.hostname)) {
            return i18n("The computer's name may hold letters, digits and dashes.");
        }
        if (!answers.rootLocked && (rootPass.text.length === 0 || rootPass.text !== rootAgain.text)) {
            return i18n("Give root a password, typed twice the same, or lock root.");
        }
        if (!answers.rootLocked && !answers.fitsAnswerFile(rootPass.text)) {
            return i18n("Root's password cannot start or end with a space here.");
        }
        if (answers.oem) {
            return "";
        }
        if (fullname.text.trim().length === 0) {
            return i18n("Type your name.");
        }
        if (!answers.fullnameOk(fullname.text)) {
            return i18n("Your name cannot hold a colon.");
        }
        if (!answers.usernameOk(answers.username)) {
            return i18n("The user name may hold lowercase letters, digits, - and _, and not start with a digit.");
        }
        if (pass.text.length === 0) {
            return i18n("Choose a password.");
        }
        if (pass.text !== again.text) {
            return i18n("The two passwords do not match.");
        }
        if (!answers.fitsAnswerFile(pass.text)) {
            return i18n("A password cannot start or end with a space here.");
        }
        if (!answers.admin && answers.rootLocked) {
            return i18n("With root locked, the account must be an administrator, or nobody could ever change the system.");
        }
        return "";
    }
    complete: accountProblem.length === 0
    problem: accountProblem

    QQC2.RadioButton {
        text: i18nc("@option:radio", "Set up my account now")
        checked: !page.answers.oem
        onToggled: page.answers.oem = false
    }
    QQC2.RadioButton {
        text: i18nc("@option:radio", "Let the first person to start it set it up (OEM)")
        checked: page.answers.oem
        onToggled: {
            page.answers.oem = true;
            page.answers.autologin = false;
        }
    }
    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit * 2
        visible: page.answers.oem
        wrapMode: Text.Wrap
        text: i18n("No account is made now. The first time the computer starts, a setup wizard asks for the language, keyboard, time zone and network, and makes an administrator's account. Use this for a computer you are preparing for someone else.")
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true

        QQC2.TextField {
            id: fullname
            Kirigami.FormData.label: i18nc("@label:textbox", "Your name:")
            visible: !page.answers.oem
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Your name")
            onTextChanged: {
                page.answers.fullname = text;
                if (!page.userEdited) {
                    page.answers.username = page.answers.suggestUsername(text);
                }
            }
        }
        QQC2.TextField {
            id: username
            Kirigami.FormData.label: i18nc("@label:textbox", "User name:")
            visible: !page.answers.oem
            Layout.fillWidth: true
            text: page.answers.username
            Accessible.name: i18nc("@label:textbox", "User name")
            onTextEdited: {
                page.userEdited = true;
                page.answers.username = text;
            }
        }
        Kirigami.PasswordField {
            id: pass
            Kirigami.FormData.label: i18nc("@label:textbox", "Password:")
            visible: !page.answers.oem
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Password")
            onTextChanged: page.answers.password = text
        }
        Kirigami.PasswordField {
            id: again
            Kirigami.FormData.label: i18nc("@label:textbox", "Again:")
            visible: !page.answers.oem
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Password, again")
        }
        QQC2.CheckBox {
            visible: !page.answers.oem
            text: i18nc("@option:check", "Administrator: may change the system and use sudo")
            checked: page.answers.admin
            onToggled: page.answers.admin = checked
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Logging in")
            visible: !page.answers.oem
        }
        QQC2.CheckBox {
            id: autologin
            visible: !page.answers.oem
            enabled: page.answers.luks
            text: i18nc("@option:check", "Log in after the disk passphrase, without the login screen")
            checked: page.answers.autologin && page.answers.luks
            onToggled: page.answers.autologin = checked
        }
        QQC2.CheckBox {
            visible: !page.answers.oem && autologin.checked
            text: i18nc("@option:check", "Start the session locked, unlocked with the password")
            checked: page.answers.autologinLock
            onToggled: page.answers.autologinLock = checked
        }
        QQC2.Label {
            visible: !page.answers.oem
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: page.answers.luks
                ? i18n("The passphrase at every start already asks who is at the computer. Without a password at login, the password store has nothing to open with and asks for its own the first time it is used.")
                : i18n("The login screen asks for the password. Logging in without it is offered only on an encrypted disk.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "This computer")
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18nc("@label:textbox", "Computer's name:")
            Layout.fillWidth: true
            text: page.answers.hostname
            Accessible.name: i18nc("@label:textbox", "Computer's name")
            onTextEdited: page.answers.hostname = text
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("@label", "Root:")
            text: i18nc("@option:check", "Lock the root account")
            checked: page.answers.rootLocked
            onToggled: page.answers.rootLocked = checked
        }
        Kirigami.PasswordField {
            id: rootPass
            Kirigami.FormData.label: i18nc("@label:textbox", "Root's password:")
            visible: !page.answers.rootLocked
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Root's password")
            onTextChanged: page.answers.rootPassword = text
        }
        Kirigami.PasswordField {
            id: rootAgain
            Kirigami.FormData.label: i18nc("@label:textbox", "Again:")
            visible: !page.answers.rootLocked
            Layout.fillWidth: true
            Accessible.name: i18nc("@label:textbox", "Root's password, again")
        }
    }
}

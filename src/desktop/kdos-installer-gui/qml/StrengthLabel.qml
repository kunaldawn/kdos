/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   StrengthLabel — how hard a password or passphrase is to guess, by length
 *
 * Under a password field on the Encryption and Account pages. Its width is
 * fixed, so the changing text never resizes the form's field column and the
 * fields beside it do not jump as a person types.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.Label {
    id: label

    property string secret
    /* "passphrase" or "password", for the advice in the short case. */
    property bool passphrase: true

    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
    Layout.maximumWidth: Kirigami.Units.gridUnit * 18
    wrapMode: Text.Wrap
    text: secret.length === 0 ? ""
        : secret.length < 8
          ? (passphrase ? i18nc("@info passphrase strength", "Short: a passphrase of several words is far harder to guess")
                        : i18nc("@info password strength", "Short: a password of several words is far harder to guess"))
        : secret.length < 16 ? i18nc("@info passphrase strength", "Fair")
        : i18nc("@info passphrase strength", "Long, good")
    color: secret.length < 8 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor
    Accessible.name: passphrase ? i18nc("@info:whatsthis", "Passphrase strength: %1", text)
                                : i18nc("@info:whatsthis", "Password strength: %1", text)
}

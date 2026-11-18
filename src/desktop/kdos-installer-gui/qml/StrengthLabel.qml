/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   StrengthLabel — how hard a password or passphrase is to guess, by length
 *
 * Under a password field on the Encryption and Account pages, shown while
 * the field is, and taking no room while it is empty, so an empty form has
 * no gap between a password and its second field. Given the `field` it
 * describes, it is as wide as that field and follows it, never narrower
 * than its fixed width; without one its width is fixed. Either way the
 * changing text never resizes the form's field column. Once it has text it
 * is as high as its longest advice wraps to at that width, so the fields
 * below it move once, at the first character, and not as a person goes on
 * typing.
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

    /* The field whose width this follows; null keeps the fixed width. */
    property Item field: null

    readonly property real fixedWidth: Kirigami.Units.gridUnit * 18
    readonly property real lineWidth: field ? Math.max(field.width, fixedWidth) : fixedWidth
    readonly property string shortAdvice: passphrase
        ? i18nc("@info passphrase strength", "Short: a passphrase of several words is far harder to guess")
        : i18nc("@info password strength", "Short: a password of several words is far harder to guess")

    /* The short advice laid out at this label's width, never shown: its
     * height is the room the label keeps for any of its texts. */
    readonly property Text longest: Text {
        width: label.lineWidth
        font: label.font
        wrapMode: Text.Wrap
        text: label.shortAdvice
    }

    /* The fixed width is a floor under the field's: the form sizes its
     * field column from this label's hints, so a width that only followed
     * the field would follow it down to the bare field's own width. */
    Layout.fillWidth: field !== null
    Layout.preferredWidth: fixedWidth
    Layout.maximumWidth: lineWidth
    Layout.minimumHeight: secret.length === 0 ? 0 : longest.implicitHeight
    Layout.preferredHeight: secret.length === 0 ? 0 : Math.max(longest.implicitHeight, implicitHeight)
    verticalAlignment: Text.AlignTop
    wrapMode: Text.Wrap
    text: secret.length === 0 ? ""
        : secret.length < 8 ? shortAdvice
        : secret.length < 16 ? i18nc("@info passphrase strength", "Fair")
        : i18nc("@info passphrase strength", "Long, good")
    color: secret.length < 8 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor
    Accessible.ignored: secret.length === 0
    Accessible.name: passphrase ? i18nc("@info:whatsthis", "Passphrase strength: %1", text)
                                : i18nc("@info:whatsthis", "Password strength: %1", text)
}

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
 * the field is. Given the `field` it describes, it is as wide as that field
 * and follows it, never narrower than its fixed width; without one its
 * width is fixed. The fixed width is the short advice's own width on one
 * line, at least 18 grid units and at most `available`, the width of the
 * page the form is in, so a form whose widest row is this label lays its
 * field column out wide enough for the advice not to wrap. A page too
 * narrow for labels and that column side by side puts the form in
 * Kirigami's one-column layout, which keeps it inside the page, and the
 * advice wraps at the page's width. Either way the changing text never
 * resizes the form's field column. It is as high as its longest advice
 * wraps to at that width whether or not anything is typed, empty text
 * included, so the second field below it stays where it is: a click aimed
 * at the second field while the first is being typed in lands in it.
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
    /* The width the form has, the page's: the most the fixed width is. */
    property real available: Infinity

    readonly property string shortAdvice: passphrase
        ? i18nc("@info passphrase strength", "Short: a passphrase of several words is far harder to guess")
        : i18nc("@info password strength", "Short: a password of several words is far harder to guess")
    /* The short advice on one line, in this label's font. */
    readonly property TextMetrics adviceMetrics: TextMetrics {
        font: label.font
        text: label.shortAdvice
    }
    readonly property real fixedWidth: Math.min(
        Math.max(Math.ceil(adviceMetrics.advanceWidth) + Kirigami.Units.smallSpacing * 2,
                 Kirigami.Units.gridUnit * 18),
        available > 0 ? available : Infinity)
    readonly property real lineWidth: field ? Math.max(field.width, fixedWidth) : fixedWidth

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
    Layout.minimumHeight: longest.implicitHeight
    Layout.preferredHeight: Math.max(longest.implicitHeight, implicitHeight)
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

/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * What kdos-box says about one key, under the control that sets it: the
 * mechanism that applies it, and every `!` line about a value it cannot
 * deliver. A setting that does nothing is shown as doing nothing, beside
 * the switch, not in a manual. The words are kdos-box's, set as rich text
 * by BoxProfile::markup so its `quoted` words and key=value settings read
 * as code rather than as terminal punctuation.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: notes

    required property string key
    readonly property string mechanism: kcm.profile.mechanisms[key] ?? ""
    readonly property var lines: kcm.profile.notes[key] ?? []

    Layout.fillWidth: true
    Layout.maximumWidth: Kirigami.Units.gridUnit * 26
    spacing: Kirigami.Units.smallSpacing
    visible: mechanism !== "" || lines.length > 0

    QQC2.Label {
        Layout.fillWidth: true
        visible: notes.mechanism !== ""
        text: i18nc("@info %1 how kdos-box applies a setting", "How it applies: %1", kcm?.profile.mechanismMarkup(notes.mechanism) ?? "")
        textFormat: Text.RichText
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }

    /* One message for every line, not a Repeater of them: a delegate the
     * Repeater destroys when Apply reloads the report is still in this
     * layout's item list while the form reads its size hint, and System
     * Settings crashes on the freed item. */
    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: notes.lines.length > 0
        type: Kirigami.MessageType.Warning
        /* `kcm` is gone before this item is when the page closes. */
        text: notes.lines.map(line => kcm?.profile.noteMarkup(line) ?? "").join("<br>")
    }
}

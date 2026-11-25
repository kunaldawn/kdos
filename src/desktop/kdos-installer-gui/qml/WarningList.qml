/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   WarningList — kinstall's warnings, as the Install and Done pages show them
 *
 * kinstall writes each warning as a clause that starts in lower case, to
 * follow whatever a front end puts before it. Here each is a sentence of its
 * own: listed once however often it was sent, one bullet each, starting with
 * a capital, and each `command` span set as code that never breaks, its
 * hyphens and spaces the non-breaking kind, so `--pending` is not split
 * across two lines. `lead`, when set, is a line above the list. Hidden while
 * there is no warning.
 */

import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.InlineMessage {
    id: list

    /* The warnings as the event stream holds them. */
    property var source: []
    property string lead

    readonly property var sentences: {
        const out = [];
        for (let i = 0; i < source.length; i++) {
            const w = source[i].length > 0 ? source[i].charAt(0).toUpperCase() + source[i].slice(1) : source[i];
            if (w.length > 0 && out.indexOf(w) < 0) {
                out.push(w);
            }
        }
        return out;
    }

    /* A warning as rich text: escaped, and each `command` span set as code. */
    function rich(text) {
        const esc = text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        return esc.replace(/`([^`]*)`/g, (m, code) =>
            "<code>" + code.replace(/-/g, "‑").replace(/ /g, "&nbsp;") + "</code>");
    }

    type: Kirigami.MessageType.Warning
    visible: sentences.length > 0
    text: (lead.length > 0 ? "<p>" + lead + "</p>" : "")
          + "<ul>" + sentences.map(w => "<li>" + list.rich(w) + "</li>").join("") + "</ul>"
}

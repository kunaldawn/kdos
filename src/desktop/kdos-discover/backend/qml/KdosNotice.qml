/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — the one line a KDOS resource needs said first
 *
 * Where an install will come from, whether it needs the network, and what a
 * signature check answered. The text and its kind are the resource's own
 * kdosNotice and kdosNoticeType, whose values are Kirigami.MessageType's.
 * ---------------------------------
 */

import QtQuick
import QtQuick.Layouts
import org.kde.discover as Discover
import org.kde.kirigami as Kirigami

Kirigami.InlineMessage {
    id: root

    required property Discover.AbstractResource resource

    Discover.Activatable.active: root.resource !== null && (root.resource.kdosNotice ?? "").length > 0

    Layout.fillWidth: true
    type: root.resource?.kdosNoticeType ?? Kirigami.MessageType.Information
    text: root.resource?.kdosNotice ?? ""

    Accessible.name: root.text
}

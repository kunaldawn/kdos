/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Lamp.qml — one lamp in the panel
 *
 * Lit, it is drawn in the negative colour, the role a capture in progress
 * takes in Breeze's own indicators; its accessible name says what is
 * capturing and what a press does.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

Kirigami.Icon {
    id: lamp

    required property string kind
    property bool lit: false
    property string actionText

    signal toggleList()

    readonly property string summary: Plasmoid.summaries[lamp.kind] ?? ""

    Layout.fillHeight: true
    Layout.preferredWidth: height
    Layout.minimumWidth: Kirigami.Units.iconSizes.small
    activeFocusOnTab: true
    isMask: true
    color: lamp.lit ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
    active: mouse.containsMouse || lamp.activeFocus

    Accessible.role: Accessible.Button
    Accessible.name: lamp.summary.length > 0 ? lamp.summary : i18ndc("kdos-plasmoid-privacy", "@info:accessible", "Nothing is capturing")
    Accessible.description: lamp.lit ? lamp.actionText : ""
    Accessible.onPressAction: lamp.press()

    function press(): void {
        if (lamp.lit) {
            Plasmoid.actOn(lamp.kind);
        } else {
            lamp.toggleList();
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            lamp.press();
            event.accepted = true;
        } else if (event.key === Qt.Key_Menu) {
            lamp.toggleList();
            event.accepted = true;
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        onClicked: mouseEvent => {
            if (mouseEvent.button === Qt.MiddleButton) {
                lamp.toggleList();
            } else {
                lamp.press();
            }
        }
    }
}

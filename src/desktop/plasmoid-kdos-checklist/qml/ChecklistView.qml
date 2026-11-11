/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ChecklistView.qml — the first-run checklist, one row an item
 *
 * Shared by the tray entry and the welcome tour's last page, so both show
 * the same list from the same probe. Each row ticks itself when the probe
 * finds the thing done; "Not Now" dismisses it everywhere.
 */

pragma ComponentBehavior: Bound

import QtQml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

ColumnLayout {
    id: view

    /* The model to show. Left unset, the view makes its own; a page that
     * already holds one passes it, so one probe serves both. */
    property ChecklistModel checklist: own.object as ChecklistModel

    Instantiator {
        id: own
        active: view.checklist === null || view.checklist === own.object
        delegate: ChecklistModel {}
    }

    spacing: Kirigami.Units.largeSpacing

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: text.length > 0
        type: Kirigami.MessageType.Warning
        text: view.checklist?.problem ?? ""
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: (view.checklist?.ready ?? false) && view.checklist.total > 0
        text: !view.checklist ? ""
            : view.checklist.finished ? i18ndc("kdos-plasmoid-checklist", "@info", "Everything on the list is done.")
            : i18ndcp("kdos-plasmoid-checklist", "@info %1 items left of %2", "%1 thing left to do of %2.", "%1 things left to do of %2.",
                      view.checklist.remaining, view.checklist.total)
        wrapMode: Text.WordWrap
    }

    Repeater {
        model: view.checklist

        delegate: RowLayout {
            id: row

            required property string itemId
            required property string title
            required property string explanation
            required property string actionText
            required property string iconName
            required property bool done
            required property bool dismissed
            required property bool optional

            Layout.fillWidth: true
            visible: !row.dismissed || row.done
            spacing: Kirigami.Units.largeSpacing

            /* The status mark is the row's check box for the keyboard and
             * assistive tools: it takes focus first in the row, so a screen
             * reader reads the title and state before the buttons, and a
             * done row, which has no buttons, can still be reached. The
             * row itself carries no accessible role, or its state would be
             * read again as context for every button inside it. */
            Item {
                id: mark

                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
                activeFocusOnTab: true

                Accessible.role: Accessible.CheckBox
                Accessible.checkable: true
                Accessible.checked: row.done
                Accessible.name: row.title
                Accessible.description: row.done ? i18ndc("kdos-plasmoid-checklist", "@info:accessible", "Done") : row.explanation

                Rectangle {
                    anchors.fill: parent
                    visible: mark.activeFocus
                    color: "transparent"
                    radius: Kirigami.Units.cornerRadius
                    border.width: 2
                    border.color: Kirigami.Theme.focusColor
                }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Kirigami.Units.iconSizes.medium
                    height: Kirigami.Units.iconSizes.medium
                    source: row.done ? "checkmark-symbolic" : row.iconName
                    isMask: true
                    color: row.done ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.textColor
                    Accessible.ignored: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 4
                    text: row.optional ? i18ndc("kdos-plasmoid-checklist", "@title %1 an item", "%1 (optional)", row.title) : row.title
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: row.done ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: !row.done
                    text: row.explanation
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    visible: !row.done
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Button {
                        text: row.actionText
                        Accessible.name: row.actionText
                        onClicked: {
                            if (!view.checklist.open(row.itemId)) {
                                failure.text = i18ndc("kdos-plasmoid-checklist", "@info %1 an item", "Could not open what “%1” needs.", row.title);
                                failure.visible = true;
                            }
                        }
                    }
                    QQC2.Button {
                        flat: true
                        text: i18ndc("kdos-plasmoid-checklist", "@action:button", "Not Now")
                        Accessible.name: i18ndc("kdos-plasmoid-checklist", "@action:button %1 an item", "Dismiss “%1”", row.title)
                        onClicked: view.checklist.dismiss(row.itemId)
                    }
                }
            }
        }
    }

    Kirigami.InlineMessage {
        id: failure
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        showCloseButton: true
        visible: false
    }

    QQC2.Button {
        Layout.alignment: Qt.AlignRight
        flat: true
        visible: (view.checklist?.ready ?? false) && view.checklist.total > 0 && view.checklist.finished
        icon.name: "edit-undo-symbolic"
        text: i18ndc("kdos-plasmoid-checklist", "@action:button", "Show Dismissed Items")
        Accessible.name: text
        onClicked: view.checklist.restore()
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   The card on one screen
 *
 * One of these is drawn on every screen while the card is open, and all of
 * them read the effect's rows and query, so typing on one screen filters
 * them all. The keyboard goes to the screen the pointer is on.
 *
 * Esc clears the search, and closes the card once the search is empty; a
 * click outside the plate closes it at once. Tab, Down and Up walk from the
 * search field through the rows, each spoken as "<action>: <keys>", and the
 * list scrolls to keep the row with the focus in sight. A changed search
 * announces how many shortcuts it leaves.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.kwin
import org.kde.plasma.components as PC3
import org.kde.plasma.extras as PlasmaExtras

import "groups.js" as Groups
import "keys.js" as CardModel

FocusScope {
    id: root

    readonly property var keycard: SceneView.effect
    readonly property var sections: CardModel.sections(keycard.allRows, Groups.groups, keycard.query)
    // As many columns as fit at a readable width.
    readonly property int columnCount: Math.max(1, Math.floor((plate.width + Kirigami.Units.gridUnit) / (Kirigami.Units.gridUnit * 23)))
    readonly property int shownCount: sections.reduce((n, section) => n + section.rows.length, 0)

    // Moves the focus to the next or previous row along the focus chain,
    // past the search field's clear button; reaching the search field again
    // gives the focus to it, so Up from the first row and Down from the last
    // end there.
    function stepRow(from, forward) {
        for (let it = from.nextItemInFocusChain(forward), n = 0; it && n < 5000; it = it.nextItemInFocusChain(forward), n++) {
            if (it === search) {
                search.forceActiveFocus(Qt.OtherFocusReason);
                return;
            }
            if (it.objectName === "kdosKeyRow") {
                it.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason);
                return;
            }
        }
    }

    // Scrolls the list just far enough to show `item` whole.
    function reveal(item) {
        const top = item.mapToItem(scroller.contentItem, 0, 0).y;
        if (top < scroller.contentY) {
            scroller.contentY = top;
        } else if (top + item.height > scroller.contentY + scroller.height) {
            scroller.contentY = Math.min(top + item.height - scroller.height, Math.max(0, scroller.contentHeight - scroller.height));
        }
    }

    function groupTitle(group) {
        switch (group) {
        case "windows":
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "Windows");
        case "desktops":
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "Desktops");
        case "launch":
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "Launch");
        case "kdos":
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "KDOS");
        case "accessibility":
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "Accessibility");
        default:
            return i18ndc("kdos-kwin-effect-keys", "@title:group", "System");
        }
    }

    focus: true
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Keys.onEscapePressed: {
        if (root.keycard.query.length > 0) {
            root.keycard.query = "";
        } else {
            root.keycard.close();
        }
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_PageDown) {
            scroller.flick(0, -scroller.height * 4);
            event.accepted = true;
        } else if (event.key === Qt.Key_PageUp) {
            scroller.flick(0, scroller.height * 4);
            event.accepted = true;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.85)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: root.keycard.close()
        }
    }

    Rectangle {
        id: plate

        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 84)
        height: parent.height - Kirigami.Units.gridUnit * 4
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.Theme.focusColor
        border.width: 1

        Accessible.role: Accessible.Dialog
        Accessible.name: heading.text

        // A click on the plate is not a click outside it.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.gridUnit
            spacing: Kirigami.Units.largeSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    id: heading

                    Layout.fillWidth: true
                    level: 1
                    text: i18ndc("kdos-kwin-effect-keys", "@title:window", "Keyboard Shortcuts")
                    elide: Text.ElideRight
                }

                PlasmaExtras.SearchField {
                    id: search

                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    focus: true
                    text: root.keycard.query
                    placeholderText: i18ndc("kdos-kwin-effect-keys", "@info:placeholder", "Search shortcuts, programs or keys…")
                    Accessible.name: i18ndc("kdos-kwin-effect-keys", "@label accessible name of the search field", "Search shortcuts")
                    onTextEdited: {
                        root.keycard.query = text;
                        announcer.restart();
                    }
                    Keys.onEscapePressed: event => {
                        event.accepted = false;
                    }
                    Keys.onDownPressed: root.stepRow(search, true)
                }

                // Spoken once typing pauses, not at every letter.
                Timer {
                    id: announcer

                    interval: 600
                    onTriggered: search.Accessible.announce(root.shownCount === 0
                        ? i18ndc("kdos-kwin-effect-keys", "@info announced after a search", "No shortcut matches")
                        : i18ndcp("kdos-kwin-effect-keys", "@info announced after a search", "%1 shortcut", "%1 shortcuts", root.shownCount))
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                PC3.ScrollView {
                    id: scrollView

                    anchors.fill: parent
                    visible: root.sections.length > 0
                    // The columns are laid out to the width left beside the
                    // vertical bar, so there is never anything to the side.
                    PC3.ScrollBar.horizontal.policy: PC3.ScrollBar.AlwaysOff

                    Flickable {
                        id: scroller

                        contentWidth: scrollView.availableWidth
                        contentHeight: columns.implicitHeight
                        flickableDirection: Flickable.VerticalFlick
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        RowLayout {
                            id: columns

                            width: scrollView.availableWidth
                            spacing: Kirigami.Units.gridUnit

                            Repeater {
                                model: CardModel.columns(root.sections, root.columnCount)

                                delegate: ColumnLayout {
                                    id: column

                                    required property var modelData

                                    Layout.alignment: Qt.AlignTop
                                    Layout.preferredWidth: (columns.width - columns.spacing * (root.columnCount - 1)) / root.columnCount
                                    Layout.maximumWidth: Layout.preferredWidth
                                    spacing: Kirigami.Units.gridUnit

                                    Repeater {
                                        model: column.modelData

                                        delegate: ColumnLayout {
                                            id: section

                                            required property var modelData

                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Accessible.role: Accessible.List
                                            Accessible.name: sectionHeading.text

                                            Kirigami.Heading {
                                                id: sectionHeading

                                                Layout.fillWidth: true
                                                level: 3
                                                color: Kirigami.Theme.linkColor
                                                text: root.groupTitle(section.modelData.group)
                                            }

                                            Repeater {
                                                model: section.modelData.rows

                                                delegate: FocusScope {
                                                    id: shortcutRow

                                                    required property var modelData

                                                    objectName: "kdosKeyRow"
                                                    Layout.fillWidth: true
                                                    implicitHeight: rowLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
                                                    activeFocusOnTab: true

                                                    Accessible.role: Accessible.ListItem
                                                    Accessible.focusable: true
                                                    Accessible.name: i18ndc("kdos-kwin-effect-keys", "@info accessible name of a shortcut: action, then its keys", "%1: %2", shortcutRow.modelData.text, shortcutRow.modelData.keys.join(i18ndc("kdos-kwin-effect-keys", "@info between two keys of one action", " or ")))

                                                    Keys.onDownPressed: root.stepRow(shortcutRow, true)
                                                    Keys.onUpPressed: root.stepRow(shortcutRow, false)
                                                    onActiveFocusChanged: {
                                                        if (activeFocus) {
                                                            root.reveal(shortcutRow);
                                                        }
                                                    }

                                                    Rectangle {
                                                        anchors.fill: parent
                                                        visible: shortcutRow.activeFocus
                                                        color: "transparent"
                                                        border.color: Kirigami.Theme.focusColor
                                                        border.width: 1
                                                        radius: Kirigami.Units.cornerRadius
                                                    }

                                                    RowLayout {
                                                        id: rowLayout

                                                        anchors.fill: parent
                                                        anchors.margins: Kirigami.Units.smallSpacing
                                                        spacing: Kirigami.Units.largeSpacing

                                                        // The keys take 45% of the row and never less than
                                                        // ten grid units; a second key wraps onto a line of
                                                        // its own, and one key wider than the column wraps
                                                        // inside its chip, so no key is ever cut short.
                                                        Flow {
                                                            id: chordFlow

                                                            Layout.preferredWidth: Math.max(Kirigami.Units.gridUnit * 10, rowLayout.width * 0.45)
                                                            Layout.alignment: Qt.AlignTop
                                                            spacing: Kirigami.Units.smallSpacing

                                                            Repeater {
                                                                model: shortcutRow.modelData.keys

                                                                delegate: Rectangle {
                                                                    id: chip

                                                                    required property string modelData

                                                                    implicitWidth: chipLabel.implicitWidth + Kirigami.Units.smallSpacing * 2
                                                                    width: Math.min(implicitWidth, chordFlow.width)
                                                                    height: chipLabel.implicitHeight + Kirigami.Units.smallSpacing
                                                                    color: Kirigami.Theme.alternateBackgroundColor
                                                                    border.color: Kirigami.Theme.disabledTextColor
                                                                    border.width: 1

                                                                    PC3.Label {
                                                                        id: chipLabel

                                                                        anchors.centerIn: parent
                                                                        width: chip.width - Kirigami.Units.smallSpacing * 2
                                                                        text: chip.modelData
                                                                        font: Kirigami.Theme.fixedWidthFont
                                                                        wrapMode: Text.Wrap
                                                                        Accessible.ignored: true
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        PC3.Label {
                                                            Layout.fillWidth: true
                                                            Layout.alignment: Qt.AlignTop
                                                            text: shortcutRow.modelData.text
                                                            wrapMode: Text.Wrap
                                                            Accessible.ignored: true
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                PlasmaExtras.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: root.sections.length === 0
                    iconName: root.keycard.status === "failed" ? "dialog-error" : "search"
                    text: {
                        if (root.keycard.status === "loading") {
                            return i18ndc("kdos-kwin-effect-keys", "@info", "Reading the shortcuts…");
                        }
                        if (root.keycard.status === "failed") {
                            return i18ndc("kdos-kwin-effect-keys", "@info", "The shortcut service did not answer");
                        }
                        if (root.keycard.query.length > 0) {
                            return i18ndc("kdos-kwin-effect-keys", "@info", "No shortcut matches “%1”", root.keycard.query);
                        }
                        return i18ndc("kdos-kwin-effect-keys", "@info", "No global shortcut is bound");
                    }
                }
            }

            PC3.Label {
                Layout.fillWidth: true
                text: i18ndc("kdos-kwin-effect-keys", "@info", "Esc closes. Change any of these in System Settings, Keyboard, Shortcuts.")
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }
        }
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   LookPage — the accent and the desk the first login starts with
 *
 * The accents are the ones kinstall accepts (`--dump look`), each shown by its
 * global theme's preview, or by a swatch painted from its own colour scheme
 * where no preview is installed. Both are changed later on the Global Theme
 * page, with `kdos theme`, or with `kdos desk`. The accent cards are one
 * radio group with one Tab stop, the chosen card, or every card while none
 * is chosen: the arrow keys move between them and Space or Enter chooses
 * one. Tab reaches each desk row and Space chooses it.
 *
 * Each desk is shown by a small drawing of its screen, made here from the
 * colour scheme's roles: KDOS with its one panel along the bottom, Dock with
 * a bar along the top and a dock floating below, Tiling with the top bar
 * and windows tiled side by side. No stock icon depicts a layout, and two
 * of them look alike at this size.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    /* A desk's screen in miniature: `bottomPanel` is KDOS's panel, `topBar`
     * the Dock and Tiling desks' bar, `dock` the floating dock, and `tiled`
     * the windows side by side; without `tiled`, one window floats. */
    component DeskPreview: Rectangle {
        id: screen
        property bool bottomPanel: false
        property bool topBar: false
        property bool dock: false
        property bool tiled: false
        readonly property real bar: Math.max(3, Math.round(height * 0.12))
        readonly property real gap: Math.max(2, Math.round(height * 0.06))

        implicitWidth: Kirigami.Units.gridUnit * 4
        implicitHeight: Kirigami.Units.gridUnit * 3
        radius: 2
        color: Kirigami.Theme.alternateBackgroundColor
        border.width: 1
        border.color: Kirigami.Theme.disabledTextColor
        Accessible.ignored: true

        /* The work area: what the bars leave of the screen. */
        Item {
            id: area
            anchors.fill: parent
            anchors.margins: screen.gap
            anchors.topMargin: screen.gap + (screen.topBar ? screen.bar : 0)
            anchors.bottomMargin: screen.gap + (screen.bottomPanel ? screen.bar : 0) + (screen.dock ? screen.bar + screen.gap : 0)
        }
        Rectangle {
            visible: !screen.tiled
            x: area.x + area.width * 0.15
            y: area.y + area.height * 0.1
            width: area.width * 0.7
            height: area.height * 0.8
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: Kirigami.Theme.textColor
        }
        Rectangle {
            visible: screen.tiled
            x: area.x
            y: area.y
            width: area.width / 2 - 1
            height: area.height
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: Kirigami.Theme.textColor
        }
        Repeater {
            model: screen.tiled ? 2 : 0
            delegate: Rectangle {
                required property int index
                x: area.x + area.width / 2 + 1
                y: area.y + index * (area.height / 2 + 1)
                width: area.width / 2 - 1
                height: area.height / 2 - 1
                color: Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: Kirigami.Theme.textColor
            }
        }
        Rectangle {
            visible: screen.topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 1
            height: screen.bar
            color: Kirigami.Theme.textColor
        }
        Rectangle {
            visible: screen.bottomPanel
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 1
            height: screen.bar
            color: Kirigami.Theme.textColor
            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 2
                width: parent.height
                height: parent.height - 2
                color: Kirigami.Theme.highlightColor
            }
        }
        Rectangle {
            visible: screen.dock
            anchors.bottom: parent.bottom
            anchors.bottomMargin: screen.gap
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.5
            height: screen.bar
            radius: height / 2
            color: Kirigami.Theme.highlightColor
        }
    }

    heading: i18nc("@title", "Look")
    readonly property var answers: Backend.answers
    readonly property var accents: Backend.look.accents || []

    onEntered: {
        if (accents.length === 0) {
            Backend.refreshLook();
        }
    }

    Kirigami.Heading {
        level: 3
        text: i18nc("@title:group", "Accent")
    }

    GridLayout {
        Layout.fillWidth: true
        columns: Math.max(2, Math.floor(page.availableWidth / (Kirigami.Units.gridUnit * 10)))
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.largeSpacing
        Accessible.role: Accessible.Grouping
        Accessible.name: i18nc("@label accessible name of the accent cards", "Accents")
        Accessible.description: i18nc("@info accessible description of the accent cards", "The arrow keys move between the accents; Space or Enter chooses one.")

        QQC2.ButtonGroup {
            id: accentGroup
            exclusive: true
        }

        Repeater {
            id: accentRepeater
            model: page.accents
            delegate: QQC2.ItemDelegate {
                id: card
                required property var modelData
                required property int index
                readonly property bool chosen: modelData.name === page.answers.theme
                readonly property var colors: Backend.accentColors(modelData.name)
                readonly property string preview: Backend.accentPreview(modelData.lookandfeel)
                readonly property string label: Backend.accentLabel(modelData.name)

                /* One preferred width for every card: the layout shares the
                 * row out from it, so a longer label never widens its
                 * column. */
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                Layout.preferredHeight: Kirigami.Units.gridUnit * 7
                /* One Tab stop for the group: the chosen card, or every
                 * card while none is chosen. */
                focusPolicy: chosen || !accentGroup.checkedButton ? Qt.StrongFocus : Qt.ClickFocus
                highlighted: chosen
                checkable: true
                checked: chosen
                QQC2.ButtonGroup.group: accentGroup
                Accessible.role: Accessible.RadioButton
                Accessible.name: i18nc("@info:whatsthis %1 accent", "%1 accent", label)

                function bindChecked() {
                    checked = Qt.binding(() => card.chosen);
                }

                /* A click checks this card and the exclusive group unchecks
                 * the one before, which cuts both from `chosen`; every card
                 * is bound again once the answer has changed, or a card
                 * chosen later by Enter would stay unchecked. */
                function choose() {
                    page.answers.theme = modelData.name;
                    for (let i = 0; i < accentRepeater.count; ++i) {
                        const c = accentRepeater.itemAt(i);
                        if (c) {
                            c.bindChecked();
                        }
                    }
                }

                /* The card `step` places away, wrapping; a Tab reason so
                 * the focus frame is drawn. */
                function focusNeighbour(step) {
                    const n = accentRepeater.count;
                    const next = accentRepeater.itemAt((((index + step) % n) + n) % n);
                    if (next) {
                        next.forceActiveFocus(Qt.TabFocusReason);
                    }
                }

                /* Tab and Backtab leave the group from whichever card has
                 * the keyboard: from past the last card or before the
                 * first, so a chosen card later in the row is not the next
                 * stop. */
                function leave(forward) {
                    const edge = accentRepeater.itemAt(forward ? accentRepeater.count - 1 : 0);
                    const next = edge ? edge.nextItemInFocusChain(forward) : null;
                    if (next) {
                        next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason);
                    }
                }

                onClicked: choose()
                Keys.onTabPressed: leave(true)
                Keys.onBacktabPressed: leave(false)
                Keys.onLeftPressed: focusNeighbour(-1)
                Keys.onUpPressed: focusNeighbour(-1)
                Keys.onRightPressed: focusNeighbour(1)
                Keys.onDownPressed: focusNeighbour(1)
                Keys.onReturnPressed: choose()
                Keys.onEnterPressed: choose()

                /* The keyboard's place, framed in the focus colour: the
                 * style's delegate focus is a shade the chosen card's
                 * highlight drowns, and a frame reads apart from it. */
                Rectangle {
                    anchors.fill: parent
                    z: 1
                    visible: card.visualFocus
                    color: "transparent"
                    border.color: Kirigami.Theme.focusColor
                    border.width: 2
                    radius: Kirigami.Units.cornerRadius
                }

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Image {
                            anchors.fill: parent
                            source: card.preview
                            fillMode: Image.PreserveAspectFit
                            visible: card.preview.length > 0
                            asynchronous: true
                        }
                        /* The scheme's own colours, where its preview is not
                         * installed: the window framed in the accent, a
                         * selection on it and its text. */
                        Rectangle {
                            anchors.fill: parent
                            visible: card.preview.length === 0
                            radius: Kirigami.Units.cornerRadius
                            color: card.colors.window || Kirigami.Theme.backgroundColor
                            border.width: 2
                            border.color: card.colors.accent || Kirigami.Theme.focusColor
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Kirigami.Units.largeSpacing
                                height: Kirigami.Units.gridUnit * 1.5
                                radius: Kirigami.Units.cornerRadius
                                color: card.colors.highlight || Kirigami.Theme.highlightColor
                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: card.label
                                    color: card.colors.highlightedText || Kirigami.Theme.highlightedTextColor
                                }
                            }
                        }
                    }
                    /* On the chosen card the delegate's background is the
                     * selection colour, so the caption takes the colour
                     * drawn on it. */
                    QQC2.Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: card.modelData.default
                            ? i18nc("@item %1 accent", "%1 (default)", card.label) : card.label
                        font.bold: card.chosen
                        color: card.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                }
            }
        }
    }

    Kirigami.Heading {
        Layout.topMargin: Kirigami.Units.largeSpacing
        level: 3
        text: i18nc("@title:group", "Desk")
    }

    Repeater {
        model: [
            { name: "kdos",   bottomPanel: true,  topBar: false, dock: false, tiled: false,
              title: Backend.deskTitle("kdos"),
              text: i18n("One panel at the bottom: the menu, the tasks, the meters, the tray and the clock.") },
            { name: "dock",   bottomPanel: false, topBar: true,  dock: true,  tiled: false,
              title: Backend.deskTitle("dock"),
              text: i18n("A bar along the top with the menu and the clock, and a floating dock below.") },
            { name: "tiling", bottomPanel: false, topBar: true,  dock: false, tiled: true,
              title: Backend.deskTitle("tiling"),
              text: i18n("A bar along the top, and windows tiled side by side as they open.") }
        ]
        delegate: QQC2.RadioDelegate {
            id: desk
            required property var modelData
            Layout.fillWidth: true
            focusPolicy: Qt.StrongFocus
            text: modelData.title + " — " + modelData.text
            checked: page.answers.desk === modelData.name
            Accessible.name: modelData.title + ": " + modelData.text
            onToggled: page.answers.desk = modelData.name
            /* The keyboard's place: the style's delegate focus is a
             * shade too faint to find on the dark ground. */
            Rectangle {
                anchors.fill: parent
                z: 1
                visible: desk.visualFocus
                color: "transparent"
                border.color: Kirigami.Theme.focusColor
                border.width: 2
                radius: Kirigami.Units.cornerRadius
            }
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                DeskPreview {
                    bottomPanel: desk.modelData.bottomPanel
                    topBar: desk.modelData.topBar
                    dock: desk.modelData.dock
                    tiled: desk.modelData.tiled
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        text: desk.modelData.title
                        font.bold: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: desk.modelData.text
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}

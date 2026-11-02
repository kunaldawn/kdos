/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   20-Desk.qml — pick your desk, and undo it
 *
 * Three desks, KDOS, Dock and Tiling, each a layout template the global
 * themes share. Picking one runs `kdos desk <name>`, which keeps a copy of
 * the panels it replaces, records the choice in kdosrc so the Global Theme
 * page lays out the same desk, and lays the new one out over the running
 * shell; Tiling also turns the auto-tiler on, the others off. `kdos desk
 * undo` puts the copy back.
 *
 * A DESK REPLACES EVERY PANEL. When `kdos desk status --json` reports the
 * panels were changed since they were laid out, the page says so before it
 * applies anything, and offers the undo after.
 *
 * The auto-tile checkbox is `kdos tiling on|off` on its own, for a KDOS or
 * Dock desk whose windows should tile anyway.
 * ---------------------------------
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

import org.kde.plasma.welcome as Welcome

import "kdos" as Kdos

Welcome.ScrollablePage {
    id: page

    heading: i18ndc("kdos-welcome", "@title:window", "Desk")
    description: i18ndc("kdos-welcome", "@info:usagetip", "Choose how your panels are laid out. You can change it again at any time from here, with “desk” in KRunner, or with the kdos desk command.")

    Kdos.Run {
        id: run
    }

    readonly property var desks: [
        {
            name: "kdos",
            title: i18ndc("kdos-welcome", "@title a desk layout", "KDOS"),
            text: i18ndc("kdos-welcome", "@info", "One panel along the bottom: Start, your desktops, pinned apps, meters, the tray and the clock. Windows float and snap to the edges.")
        },
        {
            name: "dock",
            title: i18ndc("kdos-welcome", "@title a desk layout", "Dock"),
            text: i18ndc("kdos-welcome", "@info", "A bar across the top with the application menu, and a dock at the bottom that moves out of a window's way.")
        },
        {
            name: "tiling",
            title: i18ndc("kdos-welcome", "@title a desk layout", "Tiling"),
            text: i18ndc("kdos-welcome", "@info", "A thin bar across the top, and every window tiled edge to edge. Turns auto-tiling on.")
        },
    ]

    property string current: ""
    property string chosen: ""
    property bool edited: false
    property bool tiling: false
    property string applied: ""
    property string backup: ""
    property string failure: ""
    property bool confirming: false

    function titleOf(name: string): string {
        const d = desks.find(d => d.name === name);
        return d ? d.title : name;
    }

    function setCurrent(name: string): void {
        page.current = name;
        if (page.chosen.length === 0) {
            page.chosen = name;
        }
    }

    function refresh(): void {
        run.json(["kdos", "desk", "status", "--json"], (code, value) => {
            if (code === 0 && value !== null) {
                page.edited = value.panels_edited === true;
                page.setCurrent(typeof value.desk === "string" ? value.desk : "");
            } else {
                run.run(["kdos", "desk"], (c, text) => page.setCurrent(c === 0 ? text.trim() : ""));
            }
        });
        run.run(["kdos", "tiling"], (code, text) => page.tiling = code === 0 && text.trim() === "on");
    }

    function apply(): void {
        page.confirming = false;
        const name = page.chosen;
        run.run(["kdos", "desk", name], (code, text) => {
            if (code !== 0) {
                page.failure = text;
                return;
            }
            page.failure = "";
            page.applied = name;
            // `kdos desk` prints the folder the replaced panels were copied
            // to as its last line.
            const lines = text.trim().split("\n");
            page.backup = lines[lines.length - 1].trim();
            page.refresh();
        });
    }

    function requestApply(): void {
        if (page.edited) {
            page.confirming = true;
            return;
        }
        apply();
    }

    Component.onCompleted: refresh()

    view: Kdos.Flick {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.failure.length > 0
            text: page.failure
            showCloseButton: true
            onVisibleChanged: if (!visible) page.failure = ""
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: page.confirming
            text: i18ndc("kdos-welcome", "@info", "Your panels were changed after they were laid out. A desk replaces every panel; a copy of these is kept, and Undo puts it back.")
            actions: [
                Kirigami.Action {
                    text: i18ndc("kdos-welcome", "@action:button", "Replace My Panels")
                    icon.name: "dialog-ok-apply"
                    onTriggered: page.apply()
                },
                Kirigami.Action {
                    text: i18ndc("kdos-welcome", "@action:button", "Keep Them")
                    icon.name: "dialog-cancel"
                    onTriggered: {
                        page.confirming = false;
                        page.chosen = page.current;
                    }
                }
            ]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Positive
            visible: page.applied.length > 0 && !run.busy
            text: page.backup.startsWith("/")
                ? i18ndc("kdos-welcome", "@info %1 is a desk name, %2 a folder", "The %1 desk is laid out. Your previous panels are kept in %2, and Undo puts them back.", page.titleOf(page.applied), page.backup)
                : i18ndc("kdos-welcome", "@info %1 is a desk name", "The %1 desk is laid out. Undo puts your previous panels back.", page.titleOf(page.applied))
            actions: [
                Kirigami.Action {
                    text: i18ndc("kdos-welcome", "@action:button", "Undo")
                    icon.name: "edit-undo"
                    onTriggered: run.run(["kdos", "desk", "undo"], (code, text) => {
                        if (code !== 0) {
                            page.failure = text;
                        }
                        page.applied = "";
                        page.backup = "";
                        page.chosen = "";
                        page.refresh();
                    })
                }
            ]
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            enabled: !run.busy

            Accessible.role: Accessible.Grouping
            Accessible.name: i18ndc("kdos-welcome", "@title:group", "Desk")

            Repeater {
                model: page.desks

                delegate: QQC2.AbstractButton {
                    id: card

                    required property var modelData

                    readonly property bool isChosen: page.chosen === modelData.name

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 1
                    padding: Kirigami.Units.largeSpacing
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    onClicked: page.chosen = modelData.name

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.title
                    Accessible.description: modelData.text
                    Accessible.checkable: true
                    Accessible.checked: isChosen

                    background: Rectangle {
                        color: card.isChosen ? Qt.alpha(Kirigami.Theme.highlightColor, 0.12) : "transparent"
                        border.width: card.isChosen ? 2 : 1
                        border.color: (card.isChosen || card.visualFocus || card.hovered)
                            ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                    }

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kdos.DeskPreview {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width * 9 / 16
                            desk: card.modelData.name
                        }

                        RowLayout {
                            Kirigami.Heading {
                                level: 3
                                text: card.modelData.title
                            }
                            QQC2.Label {
                                visible: page.current === card.modelData.name
                                text: i18ndc("kdos-welcome", "@info:status the desk in use", "in use")
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: card.modelData.text
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            QQC2.CheckBox {
                text: i18ndc("kdos-welcome", "@option:check", "Auto-tile windows")
                checked: page.tiling
                enabled: !run.busy
                onToggled: run.run(["kdos", "tiling", checked ? "on" : "off"], (code, text) => {
                    if (code !== 0) {
                        page.failure = text;
                    }
                    page.refresh();
                })
                Accessible.name: text
                Accessible.description: i18ndc("kdos-welcome", "@info", "Arrange every new window into tiles instead of letting it float")
            }

            Item {
                Layout.fillWidth: true
            }

            QQC2.Button {
                text: page.chosen === page.current && page.current.length > 0
                    ? i18ndc("kdos-welcome", "@action:button", "Lay Out Again")
                    : i18ndc("kdos-welcome", "@action:button %1 is a desk name", "Use %1", page.titleOf(page.chosen))
                icon.name: "dialog-ok-apply"
                enabled: page.chosen.length > 0 && !run.busy && !page.confirming
                onClicked: page.requestApply()
                Accessible.name: text
            }
        }
    }
}

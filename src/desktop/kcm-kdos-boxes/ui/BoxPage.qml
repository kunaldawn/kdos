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
 * One box: its lifecycle, its colour, and its permissions in the order a
 * person asks about them — network, files, devices and graphics, resources,
 * what it may do on the desktop, and the rest of its isolation.
 *
 * EVERY CONTROL IS ONE PROFILE KEY, and its value is profile.values[key],
 * written by Apply through `kdos-box profile`. Under each control is what
 * kdos-box says about that key: how it is applied and any `!` line about a
 * value it cannot deliver.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as QtDialogs
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: page

    readonly property var values: kcm.profile.values
    readonly property var box: kcm.current
    readonly property bool running: box.running === true
    readonly property bool created: box.created === true
    readonly property bool idle: !kcm.busy

    title: box.displayName !== undefined ? box.displayName : kcm.currentBox

    function keyLabel(key: string): string {
        switch (key) {
        case "network": return i18nc("@item a box setting", "network");
        case "home": return i18nc("@item a box setting", "home folder");
        case "persistence": return i18nc("@item a box setting", "what it writes");
        case "devices": return i18nc("@item a box setting", "devices");
        case "gpu": return i18nc("@item a box setting", "graphics card");
        case "memory": return i18nc("@item a box setting", "memory limit");
        case "cpus": return i18nc("@item a box setting", "processor limit");
        case "pids": return i18nc("@item a box setting", "process limit");
        case "ipc": return i18nc("@item a box setting", "shared memory");
        case "processes": return i18nc("@item a box setting", "process list");
        case "init": return i18nc("@item a box setting", "init process");
        }
        return key;
    }

    /* Controls write through these from onCheckedChanged, not from the
     * toggled signal: a screen reader's Toggle action changes `checked`
     * without emitting `toggled`, and an edit made that way must reach the
     * profile too. A value equal to the one held is not written, so the
     * change a binding makes on load is not an edit. */
    function setValue(key: string, value: var): void {
        if (page.values[key] !== value) {
            page.values[key] = value;
        }
    }

    function setGrant(id: string, on: bool): void {
        const held = Array.from(page.values.grant ?? []);
        if (held.includes(id) === on) {
            return;
        }
        const list = held.filter(g => g !== id);
        if (on) {
            list.push(id);
        }
        page.values.grant = list;
    }

    header: ColumnLayout {
        spacing: 0

        /* Shown by the module's messageChanged, not bound: the close button
         * assigns `visible`, which would end a binding for good. */
        Kirigami.InlineMessage {
            id: resultMessage
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            type: kcm.messageType >= 0 ? kcm.messageType : Kirigami.MessageType.Information
            text: kcm.message
            showCloseButton: true
            Component.onCompleted: visible = kcm.message !== ""
        }
        Connections {
            target: kcm
            function onMessageChanged(): void {
                resultMessage.visible = kcm.message !== "";
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.busy
            type: Kirigami.MessageType.Information
            icon.name: "view-refresh"
            text: kcm.progressText
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.needsRecreate
            type: Kirigami.MessageType.Information
            icon.name: "view-refresh"
            text: i18nc("@info %1 list of settings", "The box still has its old %1. These settings apply when the box is created, so create it again to apply them now.",
                        kcm.profile.pendingRecreate.map(k => page.keyLabel(k)).join(", "))
            actions: [
                Kirigami.Action {
                    text: i18nc("@action:button", "Recreate…")
                    icon.name: "view-refresh"
                    enabled: page.idle && !kcm.needsSave
                    onTriggered: recreateDialog.open()
                }
            ]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: page.running
            type: page.box.confinement === "confined" ? Kirigami.MessageType.Positive
                : page.box.confinement === "unconfined" ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Information
            icon.name: page.box.confinement === "unconfined" ? "security-low" : page.box.confinement === "confined" ? "security-high" : "security-medium"
            readonly property string verdict: page.box.confinement === "confined"
                ? i18nc("@info", "Confined. Its windows are tagged, its runtime folder holds no way to the session's own display or bus, its bus is filtered and no X11 display is within its reach.")
                : page.box.confinement === "tagged"
                ? i18nc("@info", "Tagged. KWin limits what its windows may do, but the box can reach past that.")
                : page.box.confinement === "unconfined"
                ? i18nc("@info", "Not confined. Its last launch had no tagged display socket, so nothing marks its windows as a box's.")
                : i18nc("@info", "The box did not record how it is confined, so this page cannot say.")
            text: (page.box.confinementReason ?? "") !== ""
                ? i18nc("@info %1 the confinement verdict, %2 why it is not confined", "%1 %2", verdict, page.box.confinementReason)
                : verdict
        }

        Repeater {
            model: kcm.profile.generalNotes

            delegate: Kirigami.InlineMessage {
                required property string modelData

                Layout.fillWidth: true
                position: Kirigami.InlineMessage.Position.Header
                visible: true
                type: Kirigami.MessageType.Warning
                /* `kcm` is gone before a delegate is when the page closes. */
                text: kcm?.profile.noteMarkup(modelData) ?? ""
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                source: page.box.iconName ?? "preferences-security"
                Layout.preferredWidth: Kirigami.Units.iconSizes.large
                Layout.preferredHeight: Kirigami.Units.iconSizes.large
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: page.title
                    wrapMode: Text.Wrap
                }
                Kirigami.SelectableLabel {
                    Layout.fillWidth: true
                    visible: text !== page.title
                    text: kcm.currentBox
                    font: Kirigami.Theme.fixedWidthFont
                    Accessible.name: i18nc("@info:whatsthis %1 box name", "Box name: %1", text)
                }
            }
        }

        Kirigami.FormLayout {
            id: form

            Layout.fillWidth: true
            enabled: kcm.currentBox !== ""

            // ── Lifecycle ─────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Lifecycle")
            }

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "State:")
                text: (page.box.disk ?? "") !== "" ? i18nc("@info %1 state, %2 size on disk", "%1 · %2 written", page.box.stateText, page.box.disk) : (page.box.stateText ?? "")
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    visible: !page.running
                    enabled: page.idle
                    text: i18nc("@action:button", "Start")
                    icon.name: "media-playback-start"
                    onClicked: kcm.startBox()
                }
                QQC2.Button {
                    visible: page.running
                    enabled: page.idle
                    text: i18nc("@action:button", "Stop")
                    icon.name: "media-playback-stop"
                    onClicked: kcm.stopBox()
                }
                QQC2.Button {
                    visible: page.running
                    enabled: page.idle
                    text: i18nc("@action:button", "Restart")
                    icon.name: "view-refresh"
                    onClicked: kcm.restartBox()
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    visible: page.created
                    enabled: page.idle && !kcm.needsSave
                    text: i18nc("@action:button", "Recreate…")
                    icon.name: "edit-redo"
                    onClicked: recreateDialog.open()
                }
                QQC2.Button {
                    visible: page.created || kcm.profile.described
                    enabled: page.idle && !kcm.needsSave
                    text: page.created ? i18nc("@action:button", "Remove…") : i18nc("@action:button", "Delete Settings…")
                    icon.name: "edit-delete"
                    onClicked: removeDialog.open()
                }
                QQC2.Button {
                    visible: (page.box.exportId ?? "") !== ""
                    enabled: page.idle && !kcm.needsSave
                    text: i18nc("@action:button", "Export as Archive…")
                    icon.name: "document-export"
                    Accessible.description: i18nc("@info:whatsthis", "Writes a .ktar archive that installs this application on another KDOS computer without a network.")
                    onClicked: exportDialog.open()
                }
            }

            // ── Colour ────────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Colour")
            }

            AccentPicker {
                Kirigami.FormData.label: i18nc("@label", "Title bar:")
                current: page.values.accent ?? "session"
                onPicked: accentId => page.values.accent = accentId
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                text: i18nc("@info", "The box's windows wear this colour, so a program in the box never looks like the same program outside it. Neutral is the plain box colour.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
            }

            // ── Network ───────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Network")
            }

            QQC2.ButtonGroup {
                id: networkGroup
            }
            QQC2.RadioButton {
                Kirigami.FormData.label: i18nc("@label", "Network:")
                QQC2.ButtonGroup.group: networkGroup
                text: i18nc("@option:radio box network", "Shared with this computer")
                checked: page.values.network === "host"
                onCheckedChanged: if (checked) page.setValue("network", "host")
            }
            QQC2.RadioButton {
                QQC2.ButtonGroup.group: networkGroup
                text: i18nc("@option:radio box network", "Its own network, with internet access")
                checked: page.values.network === "private"
                onCheckedChanged: if (checked) page.setValue("network", "private")
            }
            QQC2.RadioButton {
                QQC2.ButtonGroup.group: networkGroup
                text: i18nc("@option:radio box network", "No network")
                checked: page.values.network === "none"
                onCheckedChanged: if (checked) page.setValue("network", "none")
            }
            KeyNotes {
                key: "network"
            }

            // ── Files ─────────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Files")
            }

            QQC2.ButtonGroup {
                id: homeGroup
            }
            QQC2.RadioButton {
                Kirigami.FormData.label: i18nc("@label", "Home folder:")
                QQC2.ButtonGroup.group: homeGroup
                text: i18nc("@option:radio", "Your home folder")
                checked: page.values.home === "shared"
                onCheckedChanged: if (checked) page.setValue("home", "shared")
            }
            QQC2.RadioButton {
                QQC2.ButtonGroup.group: homeGroup
                text: i18nc("@option:radio", "A home folder of its own")
                checked: page.values.home === "private"
                onCheckedChanged: if (checked) page.setValue("home", "private")
            }
            KeyNotes {
                key: "home"
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "What it writes:")
                Accessible.name: i18nc("@label:listbox", "What the box writes")
                textRole: "text"
                valueRole: "value"
                model: [
                    { value: "persistent", text: i18nc("@item:inlistbox box persistence", "Is kept") },
                    { value: "ephemeral", text: i18nc("@item:inlistbox box persistence", "Is discarded with the box when it stops") },
                    { value: "frozen", text: i18nc("@item:inlistbox box persistence", "Is discarded when it stops; the box is kept") }
                ]
                currentIndex: count > 0 ? indexOfValue(page.values.persistence) : -1
                onActivated: page.values.persistence = currentValue
            }
            KeyNotes {
                key: "persistence"
            }

            // ── Devices and graphics ─────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Devices and Graphics")
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18nc("@label", "Devices:")
                text: i18nc("@option:check", "Keep this computer's devices out of the box")
                checked: page.values.devices === "private"
                onCheckedChanged: page.setValue("devices", checked ? "private" : "shared")
            }
            KeyNotes {
                key: "devices"
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Graphics card")
                checked: page.values.gpu === "yes"
                onCheckedChanged: page.setValue("gpu", checked ? "yes" : "no")
            }
            KeyNotes {
                key: "gpu"
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Sound")
                checked: page.values.audio === "yes"
                onCheckedChanged: page.setValue("audio", checked ? "yes" : "no")
            }
            KeyNotes {
                key: "audio"
            }
            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "Drawing:")
                Accessible.name: i18nc("@label:listbox", "How the box's applications draw")
                textRole: "text"
                valueRole: "value"
                model: [
                    { value: "auto", text: i18nc("@item:inlistbox", "On the graphics card when there is one") },
                    { value: "gpu", text: i18nc("@item:inlistbox", "On the graphics card") },
                    { value: "software", text: i18nc("@item:inlistbox", "On the processor only") }
                ]
                currentIndex: count > 0 ? indexOfValue(page.values.render) : -1
                onActivated: page.values.render = currentValue
            }
            KeyNotes {
                key: "render"
            }

            // ── Resources ─────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Resources")
            }

            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Memory limit:")
                Accessible.name: i18nc("@label:spinbox", "Memory limit")
                from: 0
                to: 1048576
                stepSize: 256
                editable: true
                value: page.values.memory ?? 0
                validator: RegularExpressionValidator {
                    regularExpression: /[^\n]*/
                }
                textFromValue: (v, locale) => kcm.profile.formatMemory(v)
                valueFromText: (text, locale) => kcm.profile.memoryFromText(text)
                onValueModified: page.values.memory = value
            }
            KeyNotes {
                key: "memory"
            }
            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Processor limit:")
                Accessible.name: i18nc("@label:spinbox", "Processor limit")
                from: 0
                to: kcm.profile.cpuCount * 10
                stepSize: 5
                editable: true
                value: page.values.cpus ?? 0
                validator: RegularExpressionValidator {
                    regularExpression: /[^\n]*/
                }
                textFromValue: (v, locale) => kcm.profile.formatCpus(v)
                valueFromText: (text, locale) => kcm.profile.cpusFromText(text)
                onValueModified: page.values.cpus = value
            }
            KeyNotes {
                key: "cpus"
            }
            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Process limit:")
                Accessible.name: i18nc("@label:spinbox", "Process limit")
                from: 0
                to: 65536
                stepSize: 64
                editable: true
                value: page.values.pids ?? 0
                textFromValue: (v, locale) => v === 0 ? i18nc("@item process limit", "No limit") : Number(v).toLocaleString(locale, "f", 0)
                valueFromText: (text, locale) => {
                    const n = parseInt(text.replace(/[^0-9]/g, ""));
                    return isNaN(n) ? 0 : n;
                }
                validator: RegularExpressionValidator {
                    regularExpression: /[^\n]*/
                }
                onValueModified: page.values.pids = value
            }
            KeyNotes {
                key: "pids"
            }
            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Stop when idle for:")
                Accessible.name: i18nc("@label:spinbox", "Stop the box when it has had no window for")
                from: 0
                to: 86400
                stepSize: 300
                editable: true
                value: page.values.autostop ?? 0
                textFromValue: (v, locale) => kcm.profile.formatDuration(v)
                valueFromText: (text, locale) => kcm.profile.durationFromText(text)
                validator: RegularExpressionValidator {
                    regularExpression: /[^\n]*/
                }
                onValueModified: page.values.autostop = value
            }
            KeyNotes {
                key: "autostop"
            }

            // ── Display and desktop ──────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Display and Desktop")
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18nc("@label", "Windows:")
                text: i18nc("@option:check", "Show its windows")
                checked: page.values.wayland === "yes"
                onCheckedChanged: page.setValue("wayland", checked ? "yes" : "no")
            }
            KeyNotes {
                key: "wayland"
            }

            QQC2.ComboBox {
                Kirigami.FormData.label: i18nc("@label:listbox", "X11 display:")
                Accessible.name: i18nc("@label:listbox", "When the box may use the X11 display")
                textRole: "text"
                valueRole: "value"
                model: [
                    { value: "yes", text: i18nc("@item:inlistbox box x11", "Always") },
                    { value: "no", text: i18nc("@item:inlistbox box x11", "Never") }
                ]
                currentIndex: count > 0 ? indexOfValue(page.values.x11) : -1
                onActivated: page.values.x11 = currentValue
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                text: i18nc("@info", "Programs written for X11 alone, such as Tk, Java and Motif ones, open no window without it. An X11 program can read every other X11 program's windows and keys, so a box that can reach the X11 display is tagged, never confined.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            KeyNotes {
                key: "x11"
            }

            ColumnLayout {
                Kirigami.FormData.label: i18nc("@label beyond its own windows, the box may also", "May also:")
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 26
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: kcm.grants

                    delegate: ColumnLayout {
                        id: grantRow

                        required property var modelData
                        readonly property bool granted: (page.values.grant ?? []).indexOf(modelData.id) >= 0

                        Layout.fillWidth: true
                        spacing: 0

                        QQC2.CheckBox {
                            text: grantRow.modelData.label
                            checked: grantRow.granted
                            Accessible.description: grantRow.modelData.description
                            onCheckedChanged: page.setGrant(grantRow.modelData.id, checked)
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            text: grantRow.modelData.description
                            wrapMode: Text.Wrap
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Kirigami.InlineMessage {
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                            visible: grantRow.granted && grantRow.modelData.desktopWide
                            type: Kirigami.MessageType.Warning
                            text: i18nc("@info", "With this, the box reaches past its own windows to the whole desktop.")
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: kcm.profile.otherGrants.length > 0
                    text: i18nc("@info %1 list of names", "Also granted, by names this page does not know: %1", kcm.profile.otherGrants.join(", "))
                    wrapMode: Text.Wrap
                }
            }
            KeyNotes {
                key: "grant"
            }

            // ── Isolation ─────────────────────────────────────────────────
            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Isolation")
            }

            QQC2.CheckBox {
                Kirigami.FormData.label: i18nc("@label", "Its own:")
                text: i18nc("@option:check", "Shared memory and message queues")
                checked: page.values.ipc === "private"
                onCheckedChanged: page.setValue("ipc", checked ? "private" : "shared")
            }
            KeyNotes {
                key: "ipc"
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Process list")
                checked: page.values.processes === "private"
                onCheckedChanged: page.setValue("processes", checked ? "private" : "shared")
            }
            KeyNotes {
                key: "processes"
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Init process")
                checked: page.values.init === "yes"
                onCheckedChanged: page.setValue("init", checked ? "yes" : "no")
            }
            KeyNotes {
                key: "init"
            }
        }
    }

    /* A dialog declared in a SimpleKCM is moved into its scrolled content
     * and centred there, out of view behind its own modal dim: each one is
     * parented to the window's overlay. */
    Kirigami.PromptDialog {
        id: recreateDialog

        parent: page.QQC2.Overlay.overlay
        title: i18nc("@title:window %1 box", "Create %1 Again?", page.title)
        subtitle: page.box.writesInside
            ? i18nc("@info", "The box is removed and created again with its current settings. It keeps what it writes inside its container, so all of that is deleted. Your home folder is not touched.")
            : i18nc("@info", "The box is removed and created again with its current settings. What it wrote is kept: its writable layer, snapshots and home live outside the container.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Recreate")
                icon.name: "edit-redo"
                onTriggered: {
                    recreateDialog.close();
                    kcm.recreateBox();
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: recreateDialog.close()
            }
        ]
    }

    Kirigami.PromptDialog {
        id: removeDialog

        parent: page.QQC2.Overlay.overlay
        title: page.created ? i18nc("@title:window %1 box", "Remove %1?", page.title) : i18nc("@title:window %1 box", "Delete the Settings of %1?", page.title)
        subtitle: !page.created
            ? i18nc("@info", "The box has never been created, so these settings are all there is of it.")
            : page.box.writesInside
            ? i18nc("@info", "Its settings are kept. It keeps what it writes inside its container, so all of that is deleted.")
            : i18nc("@info", "Its settings, writable layer, snapshots and home are kept, and it is created again the next time its application starts.")
        dialogType: Kirigami.PromptDialog.Warning
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: page.created ? i18nc("@action:button", "Remove") : i18nc("@action:button", "Delete Settings")
                icon.name: "edit-delete"
                onTriggered: {
                    removeDialog.close();
                    kcm.removeBox();
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: removeDialog.close()
            }
        ]
    }

    QtDialogs.FileDialog {
        id: exportDialog

        title: i18nc("@title:window", "Export the Application")
        fileMode: QtDialogs.FileDialog.SaveFile
        defaultSuffix: "ktar"
        nameFilters: [i18nc("@item:inlistbox file type", "KDOS application archives (*.ktar)")]
        onAccepted: kcm.exportBox(selectedFile)
    }
}

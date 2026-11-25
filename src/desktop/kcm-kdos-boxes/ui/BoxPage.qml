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
        case "base": return i18nc("@item a box setting", "base system");
        case "image": return i18nc("@item a box setting", "base image");
        }
        return key;
    }

    /* The labels of these settings as one list read in a sentence: a single
     * label as it is, two or more with "and" before the last. */
    function settingsList(keys: list<string>): string {
        const labels = keys.map(k => page.keyLabel(k));
        if (labels.length < 2) {
            return labels.join("");
        }
        return i18nc("@item:inlist the last two settings of a list", "%1 and %2",
                     labels.slice(0, -1).join(i18nc("@item:inlist list separator", ", ")),
                     labels[labels.length - 1]);
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

    /* The desktop style's SpinBox assigns `value` itself as text is typed,
     * which ends a `value:` binding: the field would then keep the typed
     * value through Reset and Defaults. Writing a limit binds it again. */
    function setLimit(box: QQC2.SpinBox, key: string): void {
        page.values[key] = box.value;
        box.value = Qt.binding(() => page.values[key] ?? 0);
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
            text: i18ncp("@info %2 list of settings",
                         "Changed since the box was created: %2. This setting applies when the box is created, so create it again to apply it now.",
                         "Changed since the box was created: %2. These settings apply when the box is created, so create it again to apply them now.",
                         kcm.recreateKeys.length, page.settingsList(kcm.recreateKeys))
            actions: [
                Kirigami.Action {
                    text: i18nc("@action:button", "Recreate…")
                    icon.name: "view-refresh"
                    enabled: page.idle && !kcm.needsSave
                    onTriggered: recreateDialog.open()
                }
            ]
        }

        /* A store box's image comes only from installing its application,
         * so neither Create nor Export as Archive, which flattens that
         * image, is offered for one whose image is gone. */
        Kirigami.InlineMessage {
            objectName: "appMissingMessage"
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: page.box.appMissing === true
            type: Kirigami.MessageType.Information
            text: i18nc("@info", "Its application is not installed. Installing it again from Discover makes this box with these settings; Delete Settings removes them.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: page.running
            type: page.box.confinement === "confined" ? Kirigami.MessageType.Positive
                : page.box.confinement === "unconfined" ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Information
            /* No record is no verdict: the plain information icon, never
             * a security level. */
            icon.name: page.box.confinement === "unconfined" ? "security-low"
                : page.box.confinement === "confined" ? "security-high"
                : page.box.confinement === "tagged" ? "security-medium"
                : "dialog-information"
            readonly property string verdict: page.box.confinement === "confined"
                ? i18nc("@info", "Confined. Its windows are tagged, its runtime folder holds no way to the session's own display or bus, its bus is filtered and no X11 display is within its reach.")
                : page.box.confinement === "tagged"
                ? i18nc("@info", "Tagged. KWin limits what its windows may do, but the box can reach past that.")
                : page.box.confinement === "unconfined"
                ? i18nc("@info", "Not confined. Its last launch had no tagged display socket, so nothing marks its windows as a box's.")
                : i18nc("@info", "Nothing has been launched in this box through KDOS yet (an exported launcher or kdos-box run), so how its windows are confined is not recorded.")
            text: (page.box.confinementReason ?? "") !== ""
                ? i18nc("@info %1 the confinement verdict, %2 why it is not confined", "%1 %2", verdict, page.box.confinementReason)
                : verdict
        }

        /* One message for every note, as in KeyNotes: a Repeater's delegate
         * destroyed when Apply reloads the report is still in this layout
         * while its size hint is read. */
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm !== null && kcm.generalNotes.length > 0
            type: Kirigami.MessageType.Warning
            /* `kcm` is gone before this item is when the page closes. */
            text: (kcm?.generalNotes ?? []).map(note => kcm.profile.noteMarkup(note)).join("<br>")
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

            /* A box with no container is made by Create: `kdos-box start`
             * refuses one, whichever way it was left. */
            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    visible: !page.created && kcm.profile.described && !page.box.appMissing
                    enabled: page.idle && !kcm.needsSave
                    text: i18nc("@action:button", "Create")
                    icon.name: "list-add"
                    Accessible.description: i18nc("@info:whatsthis", "Makes the box's container from its settings.")
                    onClicked: kcm.createBox()
                }
                QQC2.Button {
                    visible: page.created && !page.running
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
                QQC2.Button {
                    visible: page.created
                    enabled: page.idle
                    text: i18nc("@action:button", "Open Terminal")
                    icon.name: "utilities-terminal"
                    Accessible.description: i18nc("@info:whatsthis", "Opens a terminal window with a shell inside the box.")
                    onClicked: kcm.openTerminal()
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
                    visible: (page.box.exportId ?? "") !== "" && !page.box.appMissing
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
                /* The wheel scrolls the page: a setting changed by the wheel
                 * passing over it while the page scrolls is changed unseen. */
                wheelEnabled: false
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
                wheelEnabled: false
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
                id: memoryBox
                wheelEnabled: false
                Kirigami.FormData.label: i18nc("@label:spinbox", "Memory limit:")
                Accessible.name: i18nc("@label:spinbox", "Memory limit")
                from: 0
                to: 1048576
                stepSize: 256
                editable: true
                value: page.values.memory ?? 0
                validator: kcm ? kcm.profile.limitValidator("memory") : null
                textFromValue: (v, locale) => kcm.profile.formatMemory(v)
                valueFromText: (text, locale) => kcm.profile.memoryFromText(text)
                onValueModified: page.setLimit(memoryBox, "memory")
            }
            KeyNotes {
                key: "memory"
            }
            QQC2.SpinBox {
                id: cpusBox
                wheelEnabled: false
                Kirigami.FormData.label: i18nc("@label:spinbox", "Processor limit:")
                Accessible.name: i18nc("@label:spinbox", "Processor limit")
                from: 0
                to: kcm.profile.cpuCount * 10
                stepSize: 5
                editable: true
                value: page.values.cpus ?? 0
                validator: kcm ? kcm.profile.limitValidator("cpus") : null
                textFromValue: (v, locale) => kcm.profile.formatCpus(v)
                valueFromText: (text, locale) => kcm.profile.cpusFromText(text)
                onValueModified: page.setLimit(cpusBox, "cpus")
            }
            KeyNotes {
                key: "cpus"
            }
            QQC2.SpinBox {
                id: pidsBox
                wheelEnabled: false
                Kirigami.FormData.label: i18nc("@label:spinbox", "Process limit:")
                Accessible.name: i18nc("@label:spinbox", "Process limit")
                from: 0
                to: 65536
                stepSize: 64
                editable: true
                value: page.values.pids ?? 0
                textFromValue: (v, locale) => kcm.profile.formatPids(v)
                valueFromText: (text, locale) => kcm.profile.pidsFromText(text)
                validator: kcm ? kcm.profile.limitValidator("pids") : null
                onValueModified: page.setLimit(pidsBox, "pids")
            }
            KeyNotes {
                key: "pids"
            }
            QQC2.SpinBox {
                id: autostopBox
                wheelEnabled: false
                Kirigami.FormData.label: i18nc("@label:spinbox", "Stop when idle for:")
                Accessible.name: i18nc("@label:spinbox", "Stop the box when it has had no window for")
                from: 0
                to: 86400
                stepSize: 300
                editable: true
                value: page.values.autostop ?? 0
                textFromValue: (v, locale) => kcm.profile.formatDuration(v)
                valueFromText: (text, locale) => kcm.profile.durationFromText(text)
                validator: kcm ? kcm.profile.limitValidator("autostop") : null
                onValueModified: page.setLimit(autostopBox, "autostop")
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
                wheelEnabled: false
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
    /* The dialogs name the box, never an application exported from it:
     * the whole box goes, with everything installed in it. Each says what
     * goes and what stays. */
    readonly property string exportsClause: (page.box.exports ?? []).length > 0
        ? i18nc("@info %1 programs exported from the box", " and the launchers exported from it (%1)", page.box.exports.join(", "))
        : ""

    Kirigami.PromptDialog {
        id: recreateDialog

        parent: page.QQC2.Overlay.overlay
        title: i18nc("@title:window %1 box name", "Create %1 again?", kcm.currentBox)
        subtitle: page.box.writesInside
            ? i18nc("@info %1 an optional clause naming exported launchers",
                    "It is removed and created again with its current settings. Goes: its container, every package installed in it%1. Stays: the files in your home folder.",
                    page.exportsClause)
            : i18nc("@info %1 an optional clause naming exported launchers",
                    "It is removed and created again with its current settings. Goes: its container%1. Stays: its writable layer, snapshots and home, which live outside the container, and the files in your home folder.",
                    page.exportsClause)
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

        readonly property string comesBack: page.box.appBox === true
            ? i18nc("@info", "It is created again the next time its application starts.")
            : i18nc("@info", "Create makes it again from its settings.")

        parent: page.QQC2.Overlay.overlay
        title: page.created ? i18nc("@title:window %1 box name", "Remove the box %1?", kcm.currentBox)
            : i18nc("@title:window %1 box name", "Delete the settings of %1?", kcm.currentBox)
        subtitle: !page.created
            ? (page.box.kept === true
               ? i18nc("@info", "The box was removed, and these settings are all that is left of it.")
               : i18nc("@info", "The box has never been created, so these settings are all there is of it."))
            : page.box.writesInside
            ? i18nc("@info %1 an optional clause naming exported launchers, %2 how the box comes back",
                    "Goes: its container, every package installed in it%1. Stays: its settings and the files in your home folder. %2",
                    page.exportsClause, comesBack)
            : i18nc("@info %1 an optional clause naming exported launchers, %2 how the box comes back",
                    "Goes: its container%1. Stays: its settings, its writable layer, snapshots and home, and the files in your home folder. %2",
                    page.exportsClause, comesBack)
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

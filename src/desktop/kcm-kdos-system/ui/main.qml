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
 * System Versions. The banners say what the next restart will do and what
 * the last one did; the two slot forms say what each slot holds; the
 * Updates form is the one setting on the page, and Apply writes it. The
 * live medium has none of them: it keeps no versions and is not updated,
 * so the page says that and offers the installer instead. An install with
 * one system slot shows that slot's form under the one-version message
 * and no form for the slot it does not have.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as QtDialogs
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 36
    implicitHeight: Kirigami.Units.gridUnit * 34

    /* The window the page is drawn in: the scene the start prompt falls
     * back into where the platform has no second window. */
    readonly property var pageWindow: Window.window
    /* A configured boot state with a slot that has no root: installed with
     * one system slot. */
    readonly property bool oneSlot: kcm.available && kcm.configured && kcm.slotList.some(s => !s.present)

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
            visible: kcm.available && kcm.trialArmed
            type: Kirigami.MessageType.Information
            icon.name: "system-reboot"
            text: i18nc("@info %1 version to try, %2 confirmed version",
                        "When restarted, the computer will start %1 once. It is kept if it starts properly; otherwise the computer goes back to %2.",
                        kcm.trialTitle, kcm.fallbackTitle)
            actions: [
                Kirigami.Action {
                    text: i18nc("@action:button", "Restart Now…")
                    icon.name: "system-reboot"
                    enabled: !kcm.busy
                    onTriggered: kcm.restartNow()
                },
                Kirigami.Action {
                    text: i18nc("@action:button %1 confirmed version", "Keep %1", kcm.fallbackTitle)
                    icon.name: "dialog-cancel"
                    enabled: !kcm.busy
                    onTriggered: kcm.cancelTrial()
                }
            ]
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.available && kcm.onTrial
            type: kcm.trialConfirmedSoon ? Kirigami.MessageType.Information : Kirigami.MessageType.Positive
            text: kcm.trialConfirmedSoon
                ? i18nc("@info %1 version on trial", "%1 is running on trial. It becomes this computer's version once this session has been up for a minute.", kcm.trialTitle)
                : i18nc("@info %1 version on trial", "%1 started properly and is kept.", kcm.trialTitle)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.available && kcm.rolledBackTitle !== ""
            type: Kirigami.MessageType.Warning
            text: i18nc("@info %1 version that failed, %2 version running", "%1 did not start properly, so KDOS went back to %2.", kcm.rolledBackTitle, kcm.fallbackTitle)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.available && kcm.jobText !== ""
            type: Kirigami.MessageType.Information
            icon.name: "system-software-update"
            text: kcm.jobProgress > 0 ? i18nc("@info %1 job description, %2 percent", "%1 (%2%)", kcm.jobText, kcm.jobProgress) : kcm.jobText
        }

        /* An update Discover prepared and did not install: it is in the
         * other slot, and nothing starts it until Install arms the trial. */
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.available && kcm.updatePrepared && !kcm.trialArmed
            type: Kirigami.MessageType.Information
            icon.name: "system-software-update"
            text: kcm.preparedVersion !== ""
                ? i18nc("@info %1 version", "KDOS %1 is ready in the other slot. Finish the update in Discover to start it at the next restart.", kcm.preparedVersion)
                : i18nc("@info", "An update is ready in the other slot. Finish it in Discover to start it at the next restart.")
            actions: [
                Kirigami.Action {
                    text: i18nc("@action:button", "Open Discover…")
                    icon.name: "plasmadiscover"
                    onTriggered: kcm.openUpdates()
                }
            ]
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit * 2
            visible: !kcm.available || !kcm.configured || root.oneSlot
            icon.name: !kcm.loaded ? "view-refresh" : !kcm.available ? "dialog-error" : kcm.live ? "media-optical" : "drive-harddisk-root"
            text: !kcm.loaded ? i18nc("@info:placeholder", "Reading the installed versions…")
                : !kcm.available ? i18nc("@info:placeholder", "The system versions service is not running")
                : kcm.live ? i18nc("@info:placeholder", "This is the live session")
                : i18nc("@info:placeholder", "This computer has one version of KDOS")
            explanation: !kcm.loaded ? ""
                : !kcm.available ? i18nc("@info", "kdos-sysupdated answers for the installed versions. Start it with “service start sysupdated” and open this page again.")
                : kcm.live ? i18nc("@info", "It runs from the installation medium, keeps no versions and is not updated. Installing KDOS gives a system that is updated into its second slot and can go back to the previous version.")
                : i18nc("@info", "It was installed with one system slot, so there is no previous version to go back to. An installation with two slots keeps the previous version after every update.")
            /* PlaceholderMessage shows its button while the action is
             * enabled, so `enabled` is what hides it off the live medium. */
            helpfulAction: Kirigami.Action {
                enabled: kcm.available && kcm.live
                text: i18nc("@action:button", "Install KDOS…")
                icon.name: "system-software-install"
                onTriggered: kcm.openInstaller()
            }
        }

        Repeater {
            model: kcm.available && kcm.configured ? kcm.slotList : []

            delegate: SlotForm {
                required property var modelData

                Layout.fillWidth: true
                visible: modelData.present
                slotInfo: modelData
                busy: kcm.busy
                twinFormLayouts: [updatesForm]
                onStartRequested: info => {
                    kcm.holdOver(root.Window.window);
                    startDialog.slotInfo = info;
                    startDialog.open();
                }
            }
        }

        Kirigami.FormLayout {
            id: updatesForm

            Layout.fillWidth: true
            visible: kcm.available && !kcm.live

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Updates")
            }

            QQC2.ComboBox {
                id: channelBox
                Kirigami.FormData.label: i18nc("@label:listbox", "Updates come from:")
                Accessible.name: i18nc("@label:listbox", "Where updates come from")
                textRole: "text"
                valueRole: "kind"
                model: [
                    { kind: "release", text: i18nc("@item:inlistbox update source", "KDOS releases") },
                    { kind: "folder", text: i18nc("@item:inlistbox update source", "A folder or removable medium") },
                    { kind: "address", text: i18nc("@item:inlistbox update source", "Another address") }
                ]
                currentIndex: count > 0 ? indexOfValue(kcm.channelKind) : -1
                onActivated: kcm.channelKind = currentValue
            }

            RowLayout {
                Kirigami.FormData.label: i18nc("@label:textbox", "Folder:")
                visible: kcm.channelKind === "folder"
                spacing: Kirigami.Units.smallSpacing

                QQC2.TextField {
                    id: folderField
                    Layout.fillWidth: true
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                    text: kcm.channelLocation
                    placeholderText: i18nc("@info:placeholder", "/run/media/kdos/KDOS-UPDATE")
                    Accessible.name: i18nc("@label:textbox", "Update folder")
                    onTextEdited: kcm.channelLocation = text
                }
                QQC2.Button {
                    text: i18nc("@action:button", "Choose…")
                    icon.name: "document-open-folder"
                    Accessible.name: i18nc("@action:button", "Choose the update folder")
                    onClicked: folderDialog.open()
                }
            }

            QQC2.TextField {
                id: addressField
                Kirigami.FormData.label: i18nc("@label:textbox", "Address:")
                visible: kcm.channelKind === "address"
                Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                text: kcm.channelLocation
                placeholderText: i18nc("@info:placeholder", "https://example.org/kdos/updates")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                Accessible.name: i18nc("@label:textbox", "Update address")
                onTextEdited: kcm.channelLocation = text
            }

            QQC2.Label {
                visible: kcm.channelError !== ""
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                text: kcm.channelError
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                /* A one-slot install has no slot to write into but the
                 * running one: kdos-sysupdated fetches only the binhost
                 * index of the tree in place and prepares nothing, so
                 * Discover installs no system update. `kdos update fetch`
                 * takes the newer tree and `kdos update apply` writes it,
                 * over the running system with no previous version to fall
                 * back to; apply alone installs nothing newer than the tree
                 * already in /ports. */
                text: root.oneSlot
                    ? i18nc("@info", "Whatever the source, an update is installed only if it carries the KDOS release signature. On this computer it is installed from a terminal: “sudo kdos update fetch” downloads it and “sudo kdos update apply” installs it. It is written over the running system and takes effect after a restart, and there is no previous version to go back to if it does not start properly.")
                    : i18nc("@info", "Whatever the source, an update is installed only if it carries the KDOS release signature. It goes into the slot not in use and starts after a restart; if it does not start properly, the computer goes back by itself.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
            }

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "Available:")
                visible: kcm.offerVersion !== ""
                Layout.fillWidth: true
                text: i18nc("@info %1 version", "KDOS %1", kcm.offerVersion)
                Accessible.name: i18nc("@info:whatsthis %1 version", "Available update: KDOS %1", kcm.offerVersion)
            }

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "Last update:")
                visible: kcm.lastError !== ""
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                text: i18nc("@info %1 the reason an update stopped", "Stopped: %1", kcm.lastError)
                textFormat: Text.PlainText
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.Wrap
                Accessible.name: i18nc("@info:whatsthis %1 reason", "The last update stopped: %1", kcm.lastError)
            }

            /* Discover installs a system update only into a second slot. */
            QQC2.Button {
                objectName: "openUpdatesButton"
                visible: !root.oneSlot
                text: kcm.offerVersion !== "" ? i18nc("@action:button", "Update in Discover…") : i18nc("@action:button", "Check for Updates in Discover…")
                icon.name: "plasmadiscover"
                Accessible.name: kcm.offerVersion !== "" ? i18nc("@action:button", "Update in Discover") : i18nc("@action:button", "Check for updates in Discover")
                onClicked: kcm.openUpdates()
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Installing software")
            }

            QQC2.ComboBox {
                id: installRouteBox
                Kirigami.FormData.label: i18nc("@label:listbox", "Install programs:")
                Accessible.name: i18nc("@label:listbox", "How programs are installed")
                textRole: "text"
                valueRole: "route"
                model: [
                    { route: "prebuilt", text: i18nc("@item:inlistbox install route", "Download them prebuilt") },
                    { route: "source", text: i18nc("@item:inlistbox install route", "Build them on this computer") }
                ]
                currentIndex: count > 0 ? indexOfValue(kcm.installRoute) : -1
                onActivated: kcm.installRoute = currentValue
            }

            QQC2.ComboBox {
                id: buildDepsBox
                Kirigami.FormData.label: i18nc("@label:listbox", "When building, what a program needs:")
                Accessible.name: i18nc("@label:listbox", "How a build gets what it needs")
                textRole: "text"
                valueRole: "route"
                model: [
                    { route: "prebuilt", text: i18nc("@item:inlistbox build dependencies", "Download it prebuilt") },
                    { route: "source", text: i18nc("@item:inlistbox build dependencies", "Build it too") }
                ]
                currentIndex: count > 0 ? indexOfValue(kcm.buildDeps) : -1
                onActivated: kcm.buildDeps = currentValue
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                text: i18nc("@info", "Discover offers both for every program: “KDOS system” downloads it, “KDOS, built on this computer” builds it. These choices are what “kdos update install” does from a terminal, and what a build gets for anything it needs. Building takes minutes to hours; a program keeps the way it came at every update.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
            }
        }
    }

    QtDialogs.FolderDialog {
        id: folderDialog
        title: i18nc("@title:window", "Choose the Update Folder")
        onAccepted: {
            const path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""));
            kcm.channelLocation = path;
        }
    }

    /* A dialog declared in a SimpleKCM is moved into its scrolled content
     * and centred there, out of view behind its own modal dim: it is
     * parented to the window's overlay.
     *
     * System Settings shows a KCM in a scene of its own beside the sidebar
     * and the footer, so a prompt drawn inside that scene blocks only the
     * page: a click on the sidebar goes through and discards the prompt
     * unanswered. The prompt opens as a modal window of its own, which
     * blocks input to every other window of the program until it is
     * answered. Its window takes the page's scene for its parent, and that
     * scene is drawn offscreen and never mapped, so kcm.holdOver() gives
     * the scene System Settings' own window for its parent before the
     * first open: the window system resolves the prompt's parent through
     * it, and KWin keeps the prompt above System Settings as its dialog
     * and gives it no taskbar entry. Minimising a modal dialog minimises
     * System Settings with it, and restoring System Settings restores the
     * prompt. kcm.holdSize() holds the prompt's window to its size, which
     * takes away its maximise button. A hold that first reaches KWin after
     * the map leaves a dead close glyph in the title bar, so it runs as Qt
     * moves the prompt into its window on each open, the one moment before
     * the window is mapped, and again once the prompt is open, at the size
     * it settled at, which is no smaller: a re-hold that shrinks the window
     * offers maximise for a moment on a mapped window. Where the platform
     * has no second window the prompt falls back into the scene.
     *
     * As a window of its own the prompt draws no dim: KWin's Dialog Parent
     * effect dims the whole of System Settings, sidebar and footer
     * included, where compositing effects run, and under software
     * rendering, where it does not load, nothing is dimmed. A dim drawn in
     * the page's scene would darken the page alone and leave the sidebar
     * bright, which reads as a sidebar still in use. Dimmed or not, the
     * whole window refuses input until the prompt is answered.
     *
     * Kirigami's Dialog takes preferredWidth as a floor under the content's
     * implicit width, which for the subtitle is its whole sentence on one
     * line; only maximumWidth caps it, and the subtitle wraps inside.
     *
     * Kirigami's own dim is a black wash and its edge is the scheme's frame
     * contrast; on a dark scheme neither shows, and the prompt reads as part
     * of the page. Drawn in the page's scene, the prompt fades the page
     * toward the window's background, which shows on a light scheme and a
     * dark one alike; the edge is drawn halfway from the view's background
     * to its text. The background replaces Kirigami's, shadow included, so
     * the dim and the edge alone set the prompt apart from the page, and
     * as a window the edge alone does: an edge colour taken closer to the
     * background leaves nothing to show where the prompt ends. As a window
     * the frame is square, since a rounded one leaves the window's own fill
     * showing at its corners; drawn in the page's own scene, it is rounded.
     * The test is the frame's window and not popupType, which stays Window
     * where the platform puts the prompt in the scene. */
    Kirigami.PromptDialog {
        id: startDialog
        objectName: "startDialog"

        parent: root.QQC2.Overlay.overlay
        popupType: QQC2.Popup.Window
        modal: true
        preferredWidth: Kirigami.Units.gridUnit * 24
        maximumWidth: Kirigami.Units.gridUnit * 24

        QQC2.Overlay.modal: Rectangle {
            objectName: "startDialogDim"
            color: Qt.alpha(Kirigami.Theme.backgroundColor, startDialogFrame.drawnIn === root.Window.window ? 0.75 : 0)

            /* The popup sets the dim's opacity as it opens and closes. */
            Behavior on opacity {
                OpacityAnimator {
                    duration: Kirigami.Units.longDuration
                    easing.type: Easing.InOutQuad
                }
            }
        }

        background: Rectangle {
            id: startDialogFrame
            objectName: "startDialogFrame"
            readonly property var drawnIn: Window.window
            onDrawnInChanged: kcm.holdSize(drawnIn, root.pageWindow)
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false

            color: Kirigami.Theme.backgroundColor
            radius: Window.window === root.Window.window ? Kirigami.Units.cornerRadius : 0
            border.width: 1
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.5)
        }

        property var slotInfo: null

        onOpened: kcm.holdSize(startDialogFrame.drawnIn, root.pageWindow)
        onClosed: kcm.releaseSize()

        /* The caption carries no version: the window system appends the
         * program's name, and a version as long as a build's would cut it
         * short. The version is the subtitle's first sentence. */
        title: i18nc("@title:window", "Start Next Time?")
        subtitle: slotInfo
            ? i18nc("@info %1 version to try, %2 confirmed version",
                    "The next restart starts %1 once. If it starts properly and this session runs for a minute, it becomes the version this computer starts. If it does not, the computer goes back to %2 by itself.",
                    slotInfo.title, kcm.fallbackTitle)
            : ""
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Start Next Time")
                icon.name: "system-reboot"
                onTriggered: {
                    kcm.startNextTime(startDialog.slotInfo.slotId);
                    startDialog.close();
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: startDialog.close()
            }
        ]
    }
}

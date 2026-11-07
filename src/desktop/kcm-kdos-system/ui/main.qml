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
 * Updates form is the one setting on the page, and Apply writes it.
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
            visible: !kcm.available || !kcm.configured
            icon.name: !kcm.loaded ? "view-refresh" : !kcm.available ? "dialog-error" : "drive-harddisk-root"
            text: !kcm.loaded ? i18nc("@info:placeholder", "Reading the installed versions…")
                : !kcm.available ? i18nc("@info:placeholder", "The system versions service is not running")
                : i18nc("@info:placeholder", "This computer has one version of KDOS")
            explanation: !kcm.loaded ? ""
                : !kcm.available ? i18nc("@info", "kdos-sysupdated answers for the installed versions. Start it with “service start sysupdated” and open this page again.")
                : i18nc("@info", "It was installed with one system slot, so there is no previous version to go back to. An installation with two slots keeps the previous version after every update.")
        }

        Repeater {
            model: kcm.available && kcm.configured ? kcm.slotList : []

            delegate: SlotForm {
                required property var modelData

                Layout.fillWidth: true
                slotInfo: modelData
                busy: kcm.busy
                twinFormLayouts: [updatesForm]
                onStartRequested: info => {
                    startDialog.slotInfo = info;
                    startDialog.open();
                }
            }
        }

        Kirigami.FormLayout {
            id: updatesForm

            Layout.fillWidth: true
            visible: kcm.available

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
                text: i18nc("@info", "Whatever the source, an update is installed only if it carries the KDOS release signature. It goes into the slot not in use and starts after a restart; if it does not start properly, the computer goes back by itself.")
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

            QQC2.Button {
                text: kcm.offerVersion !== "" ? i18nc("@action:button", "Update in Discover…") : i18nc("@action:button", "Check for Updates in Discover…")
                icon.name: "plasmadiscover"
                Accessible.name: kcm.offerVersion !== "" ? i18nc("@action:button", "Update in Discover") : i18nc("@action:button", "Check for updates in Discover")
                onClicked: kcm.openUpdates()
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

    Kirigami.PromptDialog {
        id: startDialog

        property var slotInfo: null

        title: slotInfo ? i18nc("@title:window %1 version", "Start %1 Next Time?", slotInfo.title) : ""
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

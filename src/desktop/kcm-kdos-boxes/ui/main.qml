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
 * The list of boxes, with the chosen box's page beside it.
 *
 * A BOX IS NEVER LEFT WITH UNAPPLIED EDITS BEHIND THE PERSON'S BACK: picking
 * another box while this one has changes asks first, because opening a box
 * reloads its profile from disk.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import org.kde.kitemmodels as KItemModels

KCM.ScrollViewKCM {
    id: root

    sidebarMode: !Kirigami.Settings.isMobile
    Kirigami.ColumnView.pinned: true
    implicitWidth: Kirigami.Units.gridUnit * 16

    property string pendingBox: ""

    function showBox(name: string): void {
        if (!kcm.openBox(name)) {
            return;
        }
        while (kcm.depth > 1) {
            kcm.pop();
        }
        kcm.push("BoxPage.qml");
    }

    function requestBox(name: string): void {
        if (name === kcm.currentBox) {
            return;
        }
        if (kcm.needsSave) {
            root.pendingBox = name;
            switchDialog.open();
            return;
        }
        showBox(name);
    }

    header: Kirigami.SearchField {
        id: search
        Accessible.name: i18nc("@label:textbox", "Search boxes")
        KeyNavigation.down: boxView
    }

    view: ListView {
        id: boxView

        Accessible.role: Accessible.List
        Accessible.name: i18nc("@label list of boxes", "Boxes")
        currentIndex: -1
        clip: true

        model: KItemModels.KSortFilterProxyModel {
            sourceModel: kcm.boxes
            filterString: search.text
            filterRoleName: "display"
            filterCaseSensitivity: Qt.CaseInsensitive
        }

        delegate: KD.SubtitleDelegate {
            id: boxDelegate

            required property var model
            required property int index

            width: ListView.view.width
            text: model.display
            subtitle: model.subtitle
            icon.name: model.decoration
            highlighted: model.name === kcm.currentBox
            Accessible.name: model.display
            Accessible.description: model.subtitle
            onClicked: {
                boxView.currentIndex = index;
                root.requestBox(model.name);
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 2
            visible: boxView.count === 0
            icon.name: kcm.boxes.error !== "" ? "dialog-error" : "preferences-security"
            text: !kcm.boxes.loaded ? i18nc("@info:placeholder", "Reading the boxes…")
                : kcm.boxes.error !== "" ? i18nc("@info:placeholder", "The boxes could not be listed")
                : search.text !== "" ? i18nc("@info:placeholder", "No box matches")
                : i18nc("@info:placeholder", "No boxes yet")
            explanation: kcm.boxes.error !== "" ? kcm.boxes.error
                : (kcm.boxes.loaded && search.text === "") ? i18nc("@info", "An application installed from Discover runs in a box of its own, and it is listed here once it is installed.")
                : ""
        }
    }

    KCM.SimpleKCM {
        id: placeholder

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            icon.name: "preferences-security"
            text: i18nc("@info:placeholder", "Choose a box to see what it may reach")
        }
    }

    /* After the page's own construction: push() asks for the finished
     * main page, which is this one. */
    Component.onCompleted: Qt.callLater(() => {
        kcm.push(placeholder);
        if (kcm.initialBox !== "") {
            root.showBox(kcm.initialBox);
        }
    })

    Connections {
        target: kcm
        function onCurrentBoxChanged(): void {
            if (kcm.currentBox === "") {
                while (kcm.depth > 1) {
                    kcm.pop();
                }
                kcm.push(placeholder);
            }
        }
        function onBoxRequested(box: string): void {
            root.requestBox(box);
        }
    }

    /* Parented to the window's overlay, as the box page's dialogs are, so
     * it is centred on the window whatever page holds it. */
    Kirigami.PromptDialog {
        id: switchDialog

        parent: root.QQC2.Overlay.overlay
        title: i18nc("@title:window", "Apply the Changes?")
        subtitle: i18nc("@info %1 box", "The settings of %1 have changes that are not applied.", kcm.currentBox)
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Apply")
                icon.name: "dialog-ok-apply"
                onTriggered: {
                    kcm.save();
                    switchDialog.close();
                    root.showBox(root.pendingBox);
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Discard")
                icon.name: "edit-delete-remove"
                onTriggered: {
                    switchDialog.close();
                    root.showBox(root.pendingBox);
                }
            },
            Kirigami.Action {
                text: i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: switchDialog.close()
            }
        ]
    }
}

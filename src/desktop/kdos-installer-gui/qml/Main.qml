/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Main — the wizard: the steps down the side, one page, Back and Next
 *
 * Every page answers into Backend.answers and says whether Next may leave it
 * (`complete`) and, when not, why (`problem`). The pages after Summary take
 * the navigation away: an install that has started has no Back.
 *
 * Alt+Left and Alt+Right (and the Back and Forward keys) are Back and Next,
 * taken by main.cpp's key filter and handed to stepBack() and stepNext();
 * Alt+Right never starts the install, which only the button's dialog does.
 * Return is left to the page, where it belongs to the field being typed in.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

Kirigami.ApplicationWindow {
    id: root

    title: i18nc("@title:window", "Install KDOS")
    width: Kirigami.Units.gridUnit * 58
    height: Kirigami.Units.gridUnit * 40
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 30

    readonly property var steps: [
        { id: "welcome",    title: i18nc("@title wizard step", "Welcome"),      page: welcomePage },
        { id: "keyboard",   title: i18nc("@title wizard step", "Keyboard"),     page: keyboardPage },
        { id: "disk",       title: i18nc("@title wizard step", "Disk"),         page: diskPage },
        { id: "encryption", title: i18nc("@title wizard step", "Encryption"),   page: encryptionPage },
        { id: "account",    title: i18nc("@title wizard step", "Account"),      page: accountPage },
        { id: "look",       title: i18nc("@title wizard step", "Look"),         page: lookPage },
        { id: "apps",       title: i18nc("@title wizard step", "Applications"), page: appsPage },
        { id: "summary",    title: i18nc("@title wizard step", "Summary"),      page: summaryPage },
        { id: "install",    title: i18nc("@title wizard step", "Install"),      page: installPage },
        { id: "done",       title: i18nc("@title wizard step", "Done"),         page: donePage }
    ]
    property int current: 0
    readonly property var currentPage: steps[current].page
    readonly property bool locked: steps[current].id === "install" || steps[current].id === "done"

    function go(index) {
        if (index < 0 || index >= steps.length) {
            return;
        }
        current = index;
        steps[index].page.entered();
    }
    /* Next, as the button and Alt+Right both take it: not off the Summary,
     * whose next step is the install and is the button's alone. */
    readonly property bool canGoNext: !locked && currentPage.complete && !Backend.busy
                                      && steps[current].id !== "summary"
    function stepBack() {
        if (!locked && current > 0) {
            go(current - 1);
        }
    }
    function stepNext() {
        if (canGoNext) {
            go(current + 1);
        }
    }
    function indexOf(id) {
        for (let i = 0; i < steps.length; i++) {
            if (steps[i].id === id) {
                return i;
            }
        }
        return -1;
    }

    Component.onCompleted: {
        Backend.refreshProbe();
        Backend.refreshLook();
        Backend.refreshApps();
        go(0);
    }

    Connections {
        target: Backend
        function onInstallFinished(ok) {
            quitDialog.close();
            root.go(root.indexOf("done"));
        }
    }

    /* Closing the window during an install would orphan nothing — kinstall
     * runs to its end as root — but would hide how it ended, so it asks.
     * While pkexec still waits for permission nothing has been written, and
     * the window may go: closing it ends pkexec, and the request with it. */
    property bool closeAnyway: false
    onClosing: close => {
        if (Backend.installing && !closeAnyway) {
            close.accepted = false;
            quitDialog.open();
        }
    }

    Kirigami.PromptDialog {
        id: quitDialog
        readonly property bool started: Backend.events.started
        title: started ? i18nc("@title:dialog", "The install is still running")
                       : i18nc("@title:dialog", "The install is waiting for permission")
        subtitle: started
            ? i18n("Closing this window does not stop it: the disk is still being written, and its log is %1. Keep the window open to see how it ends.", Backend.logPath)
            : i18n("The install has not started and nothing has been written. Closing the window now cancels it.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Keep Watching")
                icon.name: "dialog-ok"
                onTriggered: quitDialog.close()
            },
            Kirigami.Action {
                visible: !quitDialog.started
                text: i18nc("@action:button", "Close Anyway")
                icon.name: "window-close"
                onTriggered: {
                    quitDialog.close();
                    root.closeAnyway = true;
                    root.close();
                }
            }
        ]
    }


    Kirigami.PromptDialog {
        id: eraseDialog
        title: i18nc("@title:dialog", "Start the install?")
        subtitle: Backend.answers.plan === "wipe"
            ? i18n("Everything on %1 will be erased. There is no undo after this.", Backend.answers.disk)
            : i18n("The root partition %1 will be formatted. Other partitions are kept.", Backend.answers.root)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18nc("@action:button", "Install")
                icon.name: "run-install"
                onTriggered: {
                    eraseDialog.close();
                    root.go(root.indexOf("install"));
                }
            }
        ]
    }

    pageStack.initialPage: Kirigami.Page {
        id: wizard
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        padding: 0

        RowLayout {
            anchors.fill: parent
            spacing: 0

            /* The steps: the done ones can be returned to, the ones ahead
             * cannot, because they have not been answered yet. Every row
             * stays enabled, because a disabled row is drawn greyed: the
             * current step would read as unavailable and the ones ahead
             * would be all but invisible. A click on a row that cannot be
             * returned to does nothing. */
            QQC2.ScrollView {
                Layout.fillHeight: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 11

                ListView {
                    id: stepList
                    model: root.steps
                    currentIndex: root.current
                    clip: true
                    Accessible.name: i18nc("@info:whatsthis", "Installation steps")
                    Accessible.role: Accessible.List

                    header: ColumnLayout {
                        width: stepList.width
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Icon {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Kirigami.Units.largeSpacing * 2
                            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                            source: "start-here-kdos"
                            fallback: "system-software-install"
                        }
                        Kirigami.Heading {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
                            level: 2
                            text: i18nc("@title", "KDOS")
                        }
                    }

                    delegate: QQC2.ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData.title
                        icon.name: index < root.current ? "checkmark"
                                 : index === root.current ? "go-next" : ""
                        readonly property bool reachable: index < root.current && !root.locked
                        highlighted: index === root.current
                        hoverEnabled: reachable
                        down: reachable && pressed
                        opacity: index > root.current ? 0.7 : 1
                        Accessible.name: index < root.current
                            ? i18nc("@info:whatsthis %1 is a step", "%1, done", modelData.title)
                            : index === root.current
                                ? i18nc("@info:whatsthis %1 is a step", "%1, current step", modelData.title)
                                : i18nc("@info:whatsthis %1 is a step", "%1, not yet", modelData.title)
                        onClicked: {
                            if (reachable) {
                                root.go(index);
                            }
                        }
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillHeight: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Kirigami.Heading {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing * 2
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    level: 1
                    text: root.currentPage.heading || root.steps[root.current].title
                    wrapMode: Text.Wrap
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                    Layout.rightMargin: Kirigami.Units.largeSpacing * 2
                    type: Kirigami.MessageType.Error
                    text: Backend.error
                    visible: Backend.error.length > 0
                }

                /* Every page stays alive while another is shown, so an
                 * answer survives going Back and Next again. */
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                    Layout.rightMargin: Kirigami.Units.largeSpacing * 2
                    currentIndex: root.current

                    WelcomePage { id: welcomePage }
                    KeyboardPage { id: keyboardPage }
                    DiskPage { id: diskPage }
                    EncryptionPage { id: encryptionPage }
                    AccountPage { id: accountPage }
                    LookPage { id: lookPage }
                    AppsPage { id: appsPage }
                    SummaryPage { id: summaryPage }
                    InstallPage { id: installPage }
                    DonePage {
                        id: donePage
                        onRetry: root.go(root.indexOf("summary"))
                    }
                }

                Kirigami.Separator {
                    Layout.fillWidth: true
                    visible: !root.locked
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing
                    visible: !root.locked
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Button {
                        text: i18nc("@action:button", "Quit")
                        icon.name: "application-exit"
                        onClicked: root.close()
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: root.currentPage.complete ? "" : (root.currentPage.problem || "")
                        color: Kirigami.Theme.neutralTextColor
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                    }
                    QQC2.Button {
                        text: i18nc("@action:button", "Back")
                        icon.name: "go-previous"
                        enabled: root.current > 0
                        onClicked: root.go(root.current - 1)
                    }
                    QQC2.Button {
                        readonly property bool last: root.steps[root.current].id === "summary"
                        text: last ? i18nc("@action:button", "Install…") : i18nc("@action:button", "Next")
                        icon.name: last ? "run-install" : "go-next"
                        enabled: root.currentPage.complete && !Backend.busy
                        onClicked: {
                            if (last) {
                                eraseDialog.open();
                            } else {
                                root.go(root.current + 1);
                            }
                        }
                    }
                }
            }
        }
    }
}

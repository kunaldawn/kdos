/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Health — `kdos doctor`, a row per check
 *
 * Laid out as kinfocenter's command-output pages are — loading, nothing to
 * show, the report; Refresh, Copy to Clipboard and a filter — with one row
 * per check instead of a block of text, so a check that needs attention is
 * marked in the negative colour and named as such to a screen reader.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.ScrollViewKCM {
    id: root

    readonly property var controller: kcm.controller
    readonly property bool hasReport: controller.ready && controller.error.length === 0 && controller.total > 0

    implicitWidth: Kirigami.Units.gridUnit * 36
    implicitHeight: Kirigami.Units.gridUnit * 30

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18ndc("kdos-kcm-health", "@action:button", "Check Again")
            enabled: root.controller.ready
            shortcut: StandardKey.Refresh
            onTriggered: root.controller.refresh()
        },
        Kirigami.Action {
            icon.name: "edit-copy-symbolic"
            text: i18ndc("kdos-kcm-health", "@action:button copies the report", "Copy to Clipboard")
            enabled: root.hasReport
            onTriggered: root.controller.copyToClipboard()
        }
    ]

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        visible: root.hasReport

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: root.controller.warnings > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Positive
            text: root.controller.warnings > 0
                ? i18ndcp("kdos-kcm-health", "@info", "%1 check needs attention.", "%1 checks need attention.", root.controller.warnings)
                : root.controller.skipped > 0
                    ? i18ndcp("kdos-kcm-health", "@info", "Everything checked is fine; %1 check could not run on this machine.", "Everything checked is fine; %1 checks could not run on this machine.", root.controller.skipped)
                    : i18ndc("kdos-kcm-health", "@info", "Everything checked is fine.")
        }

        Kirigami.SearchField {
            id: filterField
            Layout.fillWidth: true
            placeholderText: i18ndc("kdos-kcm-health", "@label placeholder", "Filter…")
            Accessible.name: i18ndc("kdos-kcm-health", "accessible name for the filter", "Filter the checks")
            Accessible.searchEdit: true
            onTextChanged: root.controller.filter = text
        }
    }

    view: ListView {
        id: list

        model: root.controller.checks
        activeFocusOnTab: true
        Accessible.name: i18ndc("kdos-kcm-health", "@label accessible name of the list", "Health checks")

        section.property: "section"
        section.delegate: Kirigami.ListSectionHeader {
            required property string section
            width: ListView.view.width
            text: section
        }

        delegate: QQC2.ItemDelegate {
            id: check

            required property string section
            required property string level
            required property string message

            readonly property bool failed: level === "warn"
            readonly property bool skipped: level === "skip"

            width: ListView.view.width
            hoverEnabled: false
            down: false

            Accessible.role: Accessible.ListItem
            Accessible.name: failed
                ? i18ndc("kdos-kcm-health", "@info accessible %1 is a check's message", "Needs attention: %1", message)
                : skipped
                    ? i18ndc("kdos-kcm-health", "@info accessible %1 is a check's message", "Not checked here: %1", message)
                    : i18ndc("kdos-kcm-health", "@info accessible %1 is a check's message", "Passed: %1", message)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    source: check.failed ? "data-error" : check.skipped ? "data-information" : "data-success"
                    Accessible.ignored: true
                }

                Kirigami.SelectableLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: check.message
                    color: check.failed ? Kirigami.Theme.negativeTextColor
                        : check.skipped ? Kirigami.Theme.disabledTextColor
                        : Kirigami.Theme.textColor
                    Accessible.ignored: true
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: root.controller.ready && !root.hasReport
            icon.name: root.controller.error.length > 0 ? "action-unavailable-symbolic" : "edit-none"
            text: root.controller.error.length > 0
                ? root.controller.error
                : i18ndc("kdos-kcm-health", "@info", "kdos doctor reported no checks")
            explanation: root.controller.explanation
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: root.hasReport && list.count === 0
            icon.name: "edit-none"
            text: i18ndc("kdos-kcm-health", "@info", "No check matches the filter")
        }

        QQC2.BusyIndicator {
            id: busy

            /* Shown only after a moment, so a doctor that answers at once
             * does not flash a spinner. */
            property bool shown: false

            anchors.centerIn: parent
            running: shown && !root.controller.ready
            visible: running
            Accessible.name: i18ndc("kdos-kcm-health", "@info accessible name", "Running the checks")

            Timer {
                interval: Kirigami.Units.veryLongDuration
                running: !root.controller.ready
                onTriggered: busy.shown = true
            }

            Connections {
                target: root.controller
                function onReadyChanged() {
                    if (root.controller.ready) {
                        busy.shown = false;
                    }
                }
            }
        }
    }
}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Look — the accent everywhere, and Phosphor mode
 *
 * A SWATCH WEARS ITS OWN SCHEME. Its fill, band and label are the window,
 * selection and text roles of the colour scheme a click applies, read by
 * the controller through KColorScheme; only the frame round it is this
 * page's own theme, so the picked one is marked in the colours the page is
 * wearing now.
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: root

    readonly property var controller: kcm.controller

    header: Kirigami.InlineMessage {
        type: Kirigami.MessageType.Error
        text: root.controller.errorMessage
        visible: text.length > 0
        showCloseButton: true
        position: Kirigami.InlineMessage.Position.Header
        onVisibleChanged: {
            if (!visible) {
                root.controller.dismissError();
            }
        }
    }

    Kirigami.FormLayout {
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@title:group", "Accent")
        }

        Flow {
            id: swatches

            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@label", "Accent:")
            Layout.preferredWidth: Kirigami.Units.gridUnit * 26
            spacing: Kirigami.Units.smallSpacing
            Accessible.role: Accessible.Grouping
            Accessible.name: i18ndc("kdos-kcm-look", "@label accessible name of the swatches", "Accents")

            Repeater {
                model: root.controller.accents

                delegate: QQC2.AbstractButton {
                    id: swatch

                    required property string accentId
                    required property string label
                    required property bool current
                    required property bool available
                    required property color windowColor
                    required property color textColor
                    required property color highlightColor

                    implicitWidth: Kirigami.Units.gridUnit * 6
                    implicitHeight: Kirigami.Units.gridUnit * 3
                    enabled: available && !root.controller.busy
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: i18ndc("kdos-kcm-look", "@action:button %1 is an accent", "%1 accent", label)
                    Accessible.checkable: true
                    Accessible.checked: current
                    Accessible.description: available
                        ? i18ndc("kdos-kcm-look", "@info:tooltip %1 is an accent", "Apply %1 everywhere", label)
                        : i18ndc("kdos-kcm-look", "@info:tooltip %1 is an accent", "The %1 color scheme is not installed", label)

                    QQC2.ToolTip.text: Accessible.description
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                    onClicked: root.controller.applyAccent(accentId)

                    background: Rectangle {
                        color: swatch.windowColor
                        opacity: swatch.available ? 1 : 0.4
                        border.width: swatch.current || swatch.visualFocus ? 2 : 1
                        border.color: swatch.current || swatch.visualFocus ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor

                        Rectangle {
                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                                margins: parent.border.width
                            }
                            height: Kirigami.Units.smallSpacing * 2
                            color: swatch.highlightColor
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: swatch.label.toUpperCase()
                            color: swatch.textColor
                            font.family: "monospace"
                            elide: Text.ElideRight
                            Accessible.ignored: true
                        }

                        Kirigami.Icon {
                            visible: swatch.current
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            source: "checkmark"
                            isMask: true
                            color: swatch.textColor
                            Accessible.ignored: true
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: root.controller.syncText.length > 0
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignTop
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                source: root.controller.outOfStep ? "emblem-warning" : "emblem-ok-symbolic"
                Accessible.ignored: true
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: root.controller.syncText
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Kirigami.Theme.negativeTextColor
            text: root.controller.syncError
        }

        QQC2.Button {
            visible: root.controller.canSync
            enabled: !root.controller.busy
            icon.name: "view-refresh"
            text: i18ndc("kdos-kcm-look", "@action:button", "Repaint Everything")
            Accessible.description: i18ndc("kdos-kcm-look", "@info:tooltip", "Carry Plasma's accent to the terminals, the boot menu, the splash and the login screen")
            onClicked: root.controller.syncNow()
        }

        RowLayout {
            visible: root.controller.busy && root.controller.busyText.length > 0
            spacing: Kirigami.Units.smallSpacing

            QQC2.BusyIndicator {
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
                running: parent.visible
                Accessible.name: root.controller.busyText
            }

            QQC2.Label {
                text: root.controller.busyText
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18ndc("kdos-kcm-look", "@info", "A click repaints Plasma, open terminals, the lock and login screens, the splash and the boot menu at once. Each accent is also a Global Theme and a color scheme on their own pages.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@title:group", "Phosphor Mode")
        }

        QQC2.Switch {
            id: phosphor

            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@label", "Phosphor mode:")
            text: i18ndc("kdos-kcm-look", "@option:check", "CRT glow, scanlines and power-on motion")
            checked: root.controller.phosphorOn
            enabled: root.controller.phosphorKnown && (root.controller.phosphorSupported || root.controller.phosphorOn) && !root.controller.busy
            Accessible.name: i18ndc("kdos-kcm-look", "@option:check accessible name", "Phosphor mode")
            onToggled: {
                root.controller.setPhosphor(checked);
                checked = Qt.binding(() => root.controller.phosphorOn);
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: root.controller.phosphorKnown && !root.controller.phosphorSupported
            wrapMode: Text.Wrap
            text: root.controller.phosphorReason.length > 0
                ? root.controller.phosphorReason
                : i18ndc("kdos-kcm-look", "@info", "Phosphor mode needs GPU rendering, and this session renders in software.")
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: !root.controller.phosphorKnown && !root.controller.busy
            wrapMode: Text.Wrap
            text: i18ndc("kdos-kcm-look", "@info", "The kdos command on this system has no Phosphor mode.")
        }

        QQC2.ButtonGroup {
            id: presets
        }

        QQC2.RadioButton {
            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@label", "Preset:")
            QQC2.ButtonGroup.group: presets
            text: i18ndc("kdos-kcm-look", "@option:radio a Phosphor mode preset", "Subtle: a soft glow and vignette")
            checked: root.controller.preset === "subtle"
            enabled: root.controller.phosphorKnown && !root.controller.busy
            onToggled: {
                if (checked) {
                    root.controller.setPreset("subtle");
                }
            }
        }

        QQC2.RadioButton {
            QQC2.ButtonGroup.group: presets
            text: i18ndc("kdos-kcm-look", "@option:radio a Phosphor mode preset", "Retro: scanlines and a curved screen (lowers contrast)")
            checked: root.controller.preset === "retro"
            enabled: root.controller.phosphorKnown && !root.controller.busy
            onToggled: {
                if (checked) {
                    root.controller.setPreset("retro");
                }
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: root.controller.phosphorKnown && root.controller.preset === "custom"
            wrapMode: Text.Wrap
            text: i18ndc("kdos-kcm-look", "@info", "Custom: tuned on the effect's own settings page. Picking a preset replaces the tuning.")
        }

        QQC2.Button {
            visible: root.controller.fineTuneAvailable
            text: i18ndc("kdos-kcm-look", "@action:button", "Fine-tune…")
            icon.name: "configure"
            enabled: root.controller.phosphorKnown && !root.controller.busy
            Accessible.description: i18ndc("kdos-kcm-look", "@info:tooltip", "Strength, scanlines and curvature, on the effect's own settings page")
            onClicked: root.controller.openFineTune()
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@label", "Fullscreen windows:")
            text: i18ndc("kdos-kcm-look", "@option:check", "Draw over fullscreen windows")
            Accessible.name: i18ndc("kdos-kcm-look", "@option:check accessible name", "Draw over fullscreen windows")
            checked: root.controller.fullscreen
            enabled: root.controller.phosphorKnown && !root.controller.busy
            onToggled: {
                root.controller.setFullscreen(checked);
                checked = Qt.binding(() => root.controller.fullscreen);
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18ndc("kdos-kcm-look", "@info", "Off leaves games and video on the direct path to the screen.")
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18ndc("kdos-kcm-look", "@label", "Terminal font:")
            text: i18ndc("kdos-kcm-look", "@option:check", "Terminus in Konsole and for fixed-width text")
            checked: root.controller.terminus
            enabled: root.controller.phosphorKnown && !root.controller.busy
            onToggled: {
                root.controller.setTerminus(checked);
                checked = Qt.binding(() => root.controller.terminus);
            }
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            visible: root.controller.reducedMotion
            wrapMode: Text.Wrap
            text: i18ndc("kdos-kcm-look", "@info", "Animations are off, so the power-on, degauss and power-off motions stay still; the glow and the scanlines do not move and stay on.")
        }

        QQC2.Label {
            Layout.preferredWidth: swatches.Layout.preferredWidth
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            text: i18ndc("kdos-kcm-look", "@info", "Turning it on with the Bone or Paper accent switches to Phosphor, and turning it off puts the accent back. Window titles and the rest of the desktop keep their font. Screenshots and screen recordings show the picture without the tube.")
        }
    }
}

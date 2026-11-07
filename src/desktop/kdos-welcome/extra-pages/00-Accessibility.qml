/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   00-Accessibility.qml — before anything else
 *
 * The first KDOS page, so a person who needs the screen read, larger text,
 * more contrast or less motion has it before the tour asks anything else.
 * Every switch writes the setting Plasma itself reads, and takes effect at
 * once:
 *
 *   screen reader   kded's screenreader module over D-Bus, the call the
 *                   Meta+Alt+S chord makes
 *   high contrast   `kdos theme` with the most legible light or dark
 *                   accent, measured from the schemes rather than named
 *   reduced motion  kdeglobals [KDE] AnimationDurationFactor 0
 *   sticky, slow and bounce keys, mouse keys
 *                   kaccessrc, which KWin's input filters watch
 *   dwell click     KWin's dwellclicker effect, loaded and remembered
 *   zoom            KWin's zoom effect is always on; the page names its
 *                   chords
 *
 * Text size is the Fonts settings page, opened from here: a font is a
 * family and a size, and a page that can only pass words to a command
 * cannot write one.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Accessibility")
    description: i18ndc("kdos-welcome", "@info:usagetip", "Make KDOS easier to see, hear and use. Everything here takes effect at once and can be changed later in System Settings → Accessibility.")

    Kdos.Run {
        id: run
    }

    Kdos.Accents {
        id: accents
    }

    property bool screenReader: false
    property bool reducedMotion: false
    property bool stickyKeys: false
    property bool slowKeys: false
    property bool bounceKeys: false
    property bool mouseKeys: false
    property bool dwellClick: false
    property string failure: ""

    readonly property string readerChord: Welcome.Utils.globalShortcutText("org.kde.screenreader.desktop", "toggleScreenReader")
    readonly property string zoomIn: Welcome.Utils.globalShortcutText("kwin", "view_zoom_in")
    readonly property string zoomOut: Welcome.Utils.globalShortcutText("kwin", "view_zoom_out")
    // globalShortcutText's word for an action with no chord.
    readonly property string unassigned: Welcome.Utils.globalShortcutText("", "")

    function done(code: int, text: string): void {
        page.failure = code === 0 ? "" : text;
        page.refresh();
    }

    function readBool(file: string, group: string, key: string, set: var): void {
        run.config(file, group, key, "false", value => set(value === "true"));
    }

    function refresh(): void {
        run.dbusBool("org.kde.kded6", "/modules/screenreader", "org.kde.screenreader.enabled", [], on => page.screenReader = on);
        run.config("kdeglobals", "KDE", "AnimationDurationFactor", "", value => page.reducedMotion = value.length > 0 && parseFloat(value) === 0);
        readBool("kaccessrc", "Keyboard", "StickyKeys", v => page.stickyKeys = v);
        readBool("kaccessrc", "Keyboard", "SlowKeys", v => page.slowKeys = v);
        readBool("kaccessrc", "Keyboard", "BounceKeys", v => page.bounceKeys = v);
        readBool("kaccessrc", "Mouse", "MouseKeys", v => page.mouseKeys = v);
        run.dbusBool("org.kde.KWin", "/Effects", "org.kde.kwin.Effects.isEffectLoaded", ["string:dwellclicker"], on => page.dwellClick = on);
    }

    function setKey(file: string, group: string, key: string, on: bool): void {
        run.writeConfig(file, group, key, on ? "true" : "false", page.done);
    }

    // The effect is remembered in kwinrc so it is on at the next login, and
    // loaded now so it is on in this one.
    function setDwell(on: bool): void {
        run.writeConfig("kwinrc", "Plugins", "dwellclickerEnabled", on ? "true" : "false", (code, text) => {
            if (code !== 0) {
                page.done(code, text);
                return;
            }
            run.dbusCall("org.kde.KWin", "/Effects", on ? "org.kde.kwin.Effects.loadEffect" : "org.kde.kwin.Effects.unloadEffect",
                         ["string:dwellclicker"], page.done);
        });
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

        Kirigami.FormLayout {
            Layout.fillWidth: true
            enabled: !run.busy

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Seeing")
            }

            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Screen reader:")
                text: i18ndc("kdos-welcome", "@option:check %1 is a keyboard shortcut", "Read the screen aloud (%1)", page.readerChord)
                checked: page.screenReader
                onToggled: run.dbusCall("org.kde.kded6", "/modules/screenreader", "org.kde.screenreader.setEnabled",
                                        ["boolean:" + (checked ? "true" : "false")], page.done)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Screen reader")
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                wrapMode: Text.WordWrap
                text: i18ndc("kdos-welcome", "@info %1 is a keyboard shortcut", "The same chord, %1, starts the screen reader on the login screen too.", page.readerChord)
            }

            QQC2.Button {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Text size:")
                text: i18ndc("kdos-welcome", "@action:button", "Make Text Larger…")
                icon.name: "preferences-desktop-font"
                onClicked: run.run(["kcmshell6", "kcm_fonts"], (code, text) => page.failure = code === 0 ? "" : text)
                Accessible.name: text
            }

            RowLayout {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "High contrast:")

                QQC2.Button {
                    readonly property string accent: accents.mostLegible(false)
                    readonly property var info: accents.byName(accent)
                    visible: accent.length > 0
                    text: i18ndc("kdos-welcome", "@action:button %1 is an accent name", "Dark: %1", info ? info.title : accent)
                    onClicked: run.run(["kdos", "theme", accent], page.done)
                    Accessible.name: i18ndc("kdos-welcome", "@action:button accessible %1 is an accent name", "Use %1, the highest-contrast dark accent", info ? info.title : accent)
                }
                QQC2.Button {
                    readonly property string accent: accents.mostLegible(true)
                    readonly property var info: accents.byName(accent)
                    visible: accent.length > 0
                    text: i18ndc("kdos-welcome", "@action:button %1 is an accent name", "Light: %1", info ? info.title : accent)
                    onClicked: run.run(["kdos", "theme", accent], page.done)
                    Accessible.name: i18ndc("kdos-welcome", "@action:button accessible %1 is an accent name", "Use %1, the highest-contrast light accent", info ? info.title : accent)
                }
            }

            QQC2.Label {
                visible: page.zoomIn !== page.unassigned
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Zoom:")
                text: i18ndc("kdos-welcome", "@info %1 and %2 are keyboard shortcuts", "%1 to zoom in, %2 to zoom out", page.zoomIn, page.zoomOut)
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Motion")
            }

            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Reduced motion:")
                text: i18ndc("kdos-welcome", "@option:check", "Turn animations and flashing off")
                checked: page.reducedMotion
                onToggled: checked
                    ? run.writeConfig("kdeglobals", "KDE", "AnimationDurationFactor", "0", page.done)
                    : run.deleteConfig("kdeglobals", "KDE", "AnimationDurationFactor", page.done)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Reduced motion")
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Keyboard and mouse")
            }

            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Sticky keys:")
                text: i18ndc("kdos-welcome", "@option:check", "Press modifier keys one at a time")
                checked: page.stickyKeys
                onToggled: page.setKey("kaccessrc", "Keyboard", "StickyKeys", checked)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Sticky keys")
            }
            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Slow keys:")
                text: i18ndc("kdos-welcome", "@option:check", "Ignore brief key presses")
                checked: page.slowKeys
                onToggled: page.setKey("kaccessrc", "Keyboard", "SlowKeys", checked)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Slow keys")
            }
            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Bounce keys:")
                text: i18ndc("kdos-welcome", "@option:check", "Ignore quickly repeated presses")
                checked: page.bounceKeys
                onToggled: page.setKey("kaccessrc", "Keyboard", "BounceKeys", checked)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Bounce keys")
            }
            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Mouse keys:")
                text: i18ndc("kdos-welcome", "@option:check", "Move the pointer with the number pad")
                checked: page.mouseKeys
                onToggled: page.setKey("kaccessrc", "Mouse", "MouseKeys", checked)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Mouse keys")
            }
            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Dwell click:")
                text: i18ndc("kdos-welcome", "@option:check", "Click by holding the pointer still")
                checked: page.dwellClick
                onToggled: page.setDwell(checked)
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Dwell click")
            }

            QQC2.Button {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Everything else:")
                text: i18ndc("kdos-welcome", "@action:button", "Accessibility Settings…")
                icon.name: "preferences-desktop-accessibility"
                onClicked: run.run(["kcmshell6", "kcm_access"], (code, text) => page.failure = code === 0 ? "" : text)
                Accessible.name: text
            }
        }
    }
}

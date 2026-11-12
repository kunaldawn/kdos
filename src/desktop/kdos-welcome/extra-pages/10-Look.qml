/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   10-Look.qml — the accent, light and dark, and Phosphor mode
 *
 * Every switch here runs the one command that owns it, so this page, the
 * KRunner verbs and System Settings cannot drift apart:
 *
 *   an accent       kdos theme <name>
 *   Phosphor mode   kdos phosphor on|off, preset subtle|retro,
 *                   terminus on|off; read with status --json
 *   light and dark  the KDE keys the Quick Settings switch uses:
 *                   kdeglobals [KDE] AutomaticLookAndFeel, and the dark
 *                   accent kept in DefaultDarkLookAndFeel
 *   Night Light     kwinrc [NightColor] Active, which KWin watches
 *   reduced motion  kdeglobals [KDE] AnimationDurationFactor 0
 *
 * PHOSPHOR MODE IS OFFERED ONCE AND OFF. It is not shown while a screen
 * reader runs or the screen is magnified (the accessibility page comes
 * first, and a flashing, curved pass is the wrong thing to offer someone
 * listening to the screen or reading it through a zoom), and when the
 * session renders in software the switch says why it cannot turn on, in
 * this page's words for the reasons `kdos phosphor status` knows.
 *
 * Everything is read again whenever the page is shown and whenever the
 * Welcome Center window comes back to the front, so an accent or a setting
 * changed elsewhere while the tour is open is shown as it is.
 * Whether a zoom is in force is KWin's list of active effects: zoom and the
 * magnifier are active exactly while they magnify.
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

    heading: i18ndc("kdos-welcome", "@title:window", "Look")
    description: i18ndc("kdos-welcome", "@info:usagetip", "KDOS comes in eight accents. Picking one repaints everything at once: the desktop, every open terminal and the lock and login screens, and on an installed system the boot menu from the next start.")

    Kdos.Run {
        id: run
    }

    property bool automatic: false      // kdeglobals [KDE] AutomaticLookAndFeel
    property var phosphor: null         // kdos phosphor status --json
    property bool screenReader: false
    property bool magnified: false
    property bool nightLight: false
    property bool reducedMotion: false
    property string failure: ""

    readonly property bool currentLight: {
        const a = Kdos.Accents.byName(Kdos.Accents.current);
        return a !== null && a.light;
    }
    // dark, light or auto
    readonly property string mode: !Kdos.Accents.ready ? "" : (automatic ? "auto" : (currentLight ? "light" : "dark"))

    // `kdos phosphor status --json`: {"on", "supported", "reason", "refusal",
    // "preset", "terminus", "previous_accent"}. A field of another type is read as
    // absent, so a reply this page does not understand offers nothing.
    readonly property bool phosphorOn: phosphor !== null && phosphor.on === true
    readonly property bool phosphorSupported: phosphor !== null && phosphor.supported === true
    readonly property string phosphorReason: phosphor !== null && typeof phosphor.reason === "string" ? phosphor.reason : ""
    readonly property string phosphorRefusal: phosphor !== null && typeof phosphor.refusal === "string" ? phosphor.refusal : ""
    // The page's own words for the two refusals `kdos phosphor` names
    // ("session" and "gpu"), the same two sentences kcm-kdos-look shows; the
    // reason of a refusal it does not name is shown as the command gave it.
    readonly property string phosphorWhy: {
        if (phosphorRefusal === "session") {
            return i18ndc("kdos-welcome", "@info", "Phosphor mode needs a running Plasma session.");
        }
        if (phosphorRefusal === "gpu" || phosphorReason.length === 0) {
            return i18ndc("kdos-welcome", "@info", "Phosphor mode needs GPU rendering, and this session renders in software.");
        }
        return phosphorReason;
    }
    // "custom" is a tuning made on the effect's own page: neither radio is
    // checked for it, and picking one replaces the tuning.
    readonly property string phosphorPreset: phosphor !== null && (phosphor.preset === "retro" || phosphor.preset === "custom") ? phosphor.preset : "subtle"
    readonly property bool phosphorTerminus: phosphor !== null && phosphor.terminus === true
    readonly property bool offerPhosphor: !screenReader && !magnified

    function report(code: int, text: string): bool {
        if (code !== 0) {
            page.failure = text;
            return false;
        }
        page.failure = "";
        return true;
    }

    function readMode(): void {
        run.config("kdeglobals", "KDE", "AutomaticLookAndFeel", "false", value => page.automatic = value === "true");
    }

    function readPhosphor(): void {
        run.readJson(["kdos", "phosphor", "status", "--json"], (code, value) => page.phosphor = value);
    }

    function refresh(): void {
        Kdos.Accents.refreshCurrent();
        readMode();
        readPhosphor();
        run.dbusBool("org.kde.kded6", "/modules/screenreader", "org.kde.screenreader.enabled", [], on => page.screenReader = on);
        run.dbusCall("org.kde.KWin", "/Effects", "org.freedesktop.DBus.Properties.Get",
                     ["string:org.kde.kwin.Effects", "string:activeEffects"],
                     (code, text) => page.magnified = code === 0 && /string "(zoom|magnifier)"/.test(text));
        run.config("kwinrc", "NightColor", "Active", "false", value => page.nightLight = value === "true");
        run.config("kdeglobals", "KDE", "AnimationDurationFactor", "", value => page.reducedMotion = value.length > 0 && parseFloat(value) === 0);
    }

    function applyAccent(name: string): void {
        run.run(["kdos", "theme", name], (code, text) => {
            report(code, text);
            refresh();
        });
    }

    // Light and dark pair the dark accent kept in DefaultDarkLookAndFeel with
    // the light one. Automatic hands the choice to Plasma's own switcher; the
    // other two turn it off first, because `kdos theme` keeps it as it is.
    function setMode(want: string): void {
        if (want === "auto") {
            run.writeConfig("kdeglobals", "KDE", "AutomaticLookAndFeel", "true", (code, text) => {
                report(code, text);
                readMode();
            });
            return;
        }
        run.writeConfig("kdeglobals", "KDE", "AutomaticLookAndFeel", "false", (code, text) => {
            if (!report(code, text)) {
                return;
            }
            if (want === "light") {
                const light = Kdos.Accents.firstOf(true);
                if (light.length > 0) {
                    applyAccent(light);
                }
                return;
            }
            run.config("kdeglobals", "KDE", "DefaultDarkLookAndFeel", "", value => {
                const m = /^org\.kdos\.([a-z][a-z0-9]*)\.desktop$/.exec(value);
                const kept = m !== null ? Kdos.Accents.byName(m[1]) : null;
                const dark = (kept !== null && !kept.light) ? kept.name : Kdos.Accents.firstOf(false);
                if (dark.length > 0) {
                    applyAccent(dark);
                }
            });
        });
    }

    function setPhosphor(argv: list<string>): void {
        run.run(["kdos", "phosphor"].concat(argv), (code, text) => {
            report(code, text);
            readPhosphor();
            Kdos.Accents.refreshCurrent();
        });
    }

    readonly property bool shown: page.isCurrentPage && Window.active
    onShownChanged: if (shown) refresh()

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

        Kirigami.Heading {
            level: 2
            text: i18ndc("kdos-welcome", "@title:group", "Accent")
        }

        GridLayout {
            id: swatches

            Layout.fillWidth: true
            columns: Math.max(1, Math.floor(width / (Kirigami.Units.gridUnit * 7.5)))
            rowSpacing: Kirigami.Units.smallSpacing
            columnSpacing: Kirigami.Units.smallSpacing
            enabled: !run.busy

            Accessible.role: Accessible.Grouping
            Accessible.name: i18ndc("kdos-welcome", "@title:group", "Accent")

            Repeater {
                model: Kdos.Accents.model

                delegate: Kdos.AccentSwatch {
                    Layout.fillWidth: true
                    current: name === Kdos.Accents.current
                    onClicked: page.applyAccent(name)
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            enabled: !run.busy

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Light and dark")
            }

            QQC2.RadioButton {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Ground:")
                text: i18ndc("kdos-welcome", "@option:radio", "Dark")
                checked: page.mode === "dark"
                onToggled: page.setMode("dark")
                Accessible.name: i18ndc("kdos-welcome", "@option:radio accessible", "Dark ground")
            }
            QQC2.RadioButton {
                text: i18ndc("kdos-welcome", "@option:radio", "Light")
                checked: page.mode === "light"
                onToggled: page.setMode("light")
                Accessible.name: i18ndc("kdos-welcome", "@option:radio accessible", "Light ground")
            }
            QQC2.RadioButton {
                text: i18ndc("kdos-welcome", "@option:radio", "Follow the time of day")
                checked: page.mode === "auto"
                onToggled: page.setMode("auto")
                Accessible.name: text
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Phosphor mode")
                visible: page.offerPhosphor
            }

            QQC2.Switch {
                id: phosphorSwitch

                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "CRT pass:")
                visible: page.offerPhosphor
                enabled: page.phosphor !== null && page.phosphorSupported
                checked: page.phosphorOn
                text: page.phosphorOn ? i18ndc("kdos-welcome", "@option:check", "On") : i18ndc("kdos-welcome", "@option:check", "Off")
                onToggled: page.setPhosphor([checked ? "on" : "off"])
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Phosphor mode")
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                visible: page.offerPhosphor
                wrapMode: Text.WordWrap
                text: i18ndc("kdos-welcome", "@info", "A CRT pass over the whole screen, with the retro accents and the power-on and power-off motion. It costs some speed: fullscreen games and video are left untouched, everything else is redrawn through the pass.")
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: page.offerPhosphor && page.phosphor !== null && !page.phosphorSupported
                type: Kirigami.MessageType.Information
                text: page.phosphorWhy
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: page.offerPhosphor && phosphorSwitch.enabled
                type: Kirigami.MessageType.Warning
                text: i18ndc("kdos-welcome", "@info", "Phosphor mode moves: the screen flashes once when the accent changes, and windows fold open and shut. Leave it off if motion or flashing affects you. Reduced motion, below, stops all of it.")
            }

            QQC2.RadioButton {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Preset:")
                visible: page.offerPhosphor
                enabled: page.phosphorOn
                text: i18ndc("kdos-welcome", "@option:radio", "Subtle: glow and vignette")
                checked: page.phosphorPreset === "subtle"
                onToggled: page.setPhosphor(["preset", "subtle"])
                Accessible.name: text
            }
            QQC2.RadioButton {
                visible: page.offerPhosphor
                enabled: page.phosphorOn
                text: i18ndc("kdos-welcome", "@option:radio", "Retro: scanlines and curve (lowers contrast)")
                checked: page.phosphorPreset === "retro"
                onToggled: page.setPhosphor(["preset", "retro"])
                Accessible.name: text
            }
            QQC2.CheckBox {
                visible: page.offerPhosphor
                enabled: page.phosphor !== null && page.phosphorSupported
                text: i18ndc("kdos-welcome", "@option:check", "Terminus for terminals and fixed-width text")
                checked: page.phosphorTerminus
                onToggled: page.setPhosphor(["terminus", checked ? "on" : "off"])
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Use the Terminus font for terminals and fixed-width text in Phosphor mode")
                Accessible.description: i18ndc("kdos-welcome", "@info accessible", "Window titles and the rest of the interface keep Noto Sans")
            }

            Item {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@title:group", "Comfort")
            }

            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Night Light:")
                checked: page.nightLight
                text: i18ndc("kdos-welcome", "@option:check", "Warm the screen at night")
                onToggled: run.writeConfig("kwinrc", "NightColor", "Active", checked ? "true" : "false", (code, text) => {
                    page.report(code, text);
                    page.refresh();
                })
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Night Light")
            }

            QQC2.Switch {
                Kirigami.FormData.label: i18ndc("kdos-welcome", "@label", "Reduced motion:")
                checked: page.reducedMotion
                text: i18ndc("kdos-welcome", "@option:check", "Turn animations off")
                onToggled: {
                    const done = (code, text) => {
                        page.report(code, text);
                        page.refresh();
                    };
                    if (checked) {
                        run.writeConfig("kdeglobals", "KDE", "AnimationDurationFactor", "0", done);
                    } else {
                        run.deleteConfig("kdeglobals", "KDE", "AnimationDurationFactor", done);
                    }
                }
                Accessible.name: i18ndc("kdos-welcome", "@option:check accessible", "Reduced motion")
            }
        }
    }
}

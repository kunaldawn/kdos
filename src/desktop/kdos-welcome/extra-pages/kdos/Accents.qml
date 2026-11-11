/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Accents.qml — the accents, each in its own colours
 *
 * THE LIST IS `kdos theme list`'s, in its order: libkcolor holds the one
 * table and kdos prints it, so a page can never offer an accent the command
 * refuses. A line is the accent's name, its theme name (KDOS-<Title>) and a
 * `*` on the one in force.
 *
 * A SWATCH IS ITS COLOUR SCHEME. Each accent's ground, ink and accent are
 * read out of /usr/share/color-schemes/KDOS<Title>.colors, the file applying
 * it installs, and nothing here spells a colour. Light and dark is KDE's own
 * rule on that ground (KLookAndFeel::colorSchemeVariant: gray from 192 is
 * light), so this page and the Global Theme page agree about every accent.
 *
 * ONE OBJECT FOR EVERY PAGE. It is a singleton (kdos/qmldir), so the list
 * and the three colour reads an accent are made once however many pages show swatches;
 * an instance per page would multiply them. refreshCurrent() re-reads only
 * which accent is in force.
 * ---------------------------------
 */

pragma Singleton

import QtQuick

QtObject {
    id: accents

    // One row an accent: name, title, scheme, ground, ink, accent (each
    // "#rrggbb", read from the scheme), light, contrast, known.
    readonly property ListModel model: ListModel {}
    property bool ready: false
    property string current: ""
    // Counts every change to the rows. ListModel.get() is not a binding, so
    // anything bound to what byName() returns reads this to be re-evaluated
    // when an accent's colours arrive.
    property int revision: 0
    readonly property Run runner: Run {}

    signal loaded()

    function refresh(): void {
        runner.run(["kdos", "theme", "list"], (code, text) => {
            if (code !== 0) {
                return;
            }
            const rows = [];
            let now = "";
            for (const raw of text.split("\n")) {
                // Drop any terminal colouring before reading the words.
                const words = raw.replace(/\u001b\[[0-9;]*m/g, "").trim().split(/\s+/).filter(w => w.length > 0);
                if (words.length === 0) {
                    continue;
                }
                const isCurrent = words[0] === "*";
                const rest = isCurrent ? words.slice(1) : words;
                if (rest.length === 0 || !/^[a-z][a-z0-9]*$/.test(rest[0])) {
                    continue;
                }
                const themeName = rest.find(w => w.startsWith("KDOS-")) ?? "";
                const title = themeName.length > 5 ? themeName.slice(5) : rest[0][0].toUpperCase() + rest[0].slice(1);
                rows.push({ name: rest[0], title: title, scheme: "KDOS" + title });
                if (isCurrent) {
                    now = rest[0];
                }
            }
            accents.model.clear();
            for (const r of rows) {
                accents.model.append({
                    name: r.name, title: r.title, scheme: r.scheme,
                    ground: "", ink: "", accent: "",
                    light: false, contrast: 0, known: false
                });
            }
            accents.current = now;
            accents.ready = true;
            accents.revision++;
            accents.loaded();
            for (let i = 0; i < rows.length; i++) {
                readColours(i);
            }
        });
    }

    function refreshCurrent(): void {
        runner.run(["kdos", "theme"], (code, text) => {
            if (code === 0) {
                accents.current = text.trim();
            }
        });
    }

    function rgb(text: string): var {
        const p = text.split(",").map(n => parseInt(n, 10));
        if (p.length < 3 || p.some(n => isNaN(n))) {
            return null;
        }
        return p;
    }

    function hex(c: var): string {
        return "#" + c.map(v => v.toString(16).padStart(2, "0")).join("");
    }

    function luminance(c: var): real {
        const lin = v => {
            const s = v / 255;
            return s <= 0.03928 ? s / 12.92 : Math.pow((s + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2]);
    }

    function contrastOf(a: var, b: var): real {
        const la = luminance(a), lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }

    function readColours(i: int): void {
        const file = "/usr/share/color-schemes/" + accents.model.get(i).scheme + ".colors";
        const got = {};
        const keys = [["ground", "Colors:Window", "BackgroundNormal"],
                      ["ink", "Colors:Window", "ForegroundNormal"],
                      ["accent", "Colors:View", "DecorationFocus"]];
        for (const [role, group, key] of keys) {
            runner.config(file, group, key, "", value => {
                got[role] = rgb(value);
                if (Object.keys(got).length < keys.length) {
                    return;
                }
                if (!got.ground || !got.ink || !got.accent) {
                    return;
                }
                const g = got.ground;
                accents.model.setProperty(i, "ground", hex(g));
                accents.model.setProperty(i, "ink", hex(got.ink));
                accents.model.setProperty(i, "accent", hex(got.accent));
                accents.model.setProperty(i, "light", Math.floor((g[0] * 11 + g[1] * 16 + g[2] * 5) / 32) >= 192);
                accents.model.setProperty(i, "contrast", contrastOf(got.ink, g));
                accents.model.setProperty(i, "known", true);
                accents.revision++;
            });
        }
    }

    // Every row, as the functions below read them. It reads `revision`, so a
    // binding that calls any of them is evaluated again when a row changes;
    // without that, a binding made before the colours arrived keeps the
    // answer it gave with none known.
    function rows(): var {
        const out = [];
        if (accents.revision < 0) {
            return out;
        }
        for (let i = 0; i < accents.model.count; i++) {
            out.push(accents.model.get(i));
        }
        return out;
    }

    // The accent with the highest text contrast among the light ones, or the
    // dark ones: what "high contrast" offers, derived rather than named.
    function mostLegible(light: bool): string {
        let best = "", score = 0;
        for (const a of rows()) {
            if (a.known && a.light === light && a.contrast > score) {
                best = a.name;
                score = a.contrast;
            }
        }
        return best;
    }

    function firstOf(light: bool): string {
        const a = rows().find(r => r.known && r.light === light);
        return a ? a.name : "";
    }

    function byName(name: string): var {
        return rows().find(r => r.name === name) ?? null;
    }

    Component.onCompleted: refresh()
}

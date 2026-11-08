/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Key Card — every global shortcut, on one searchable card
 *
 * THE CARD IS READ FROM kglobalacceld EACH TIME IT OPENS, never from a file
 * and never cached: a chord changed on the Shortcuts page is on the card the
 * next time it opens, with no restart. allComponents() names every
 * component, and each component's allShortcutInfos() gives its actions with
 * their current keys; DBusCall unwraps the structs into plain lists. KWin's
 * own isScriptLoaded() is asked about each script keys.js names, because
 * kglobalacceld goes on reporting an unloaded script's keys.
 *
 * Meta+F1 is this effect's own key. Meta+/ is the same action's second key,
 * which the shipped kglobalshortcutsrc gives it: a ShortcutHandler registers
 * one key, and one action with two keys is one row on the Shortcuts page.
 * ---------------------------------
 */

import QtQuick
import org.kde.kwin

import "groups.js" as Groups
import "keys.js" as CardModel

SceneEffect {
    id: effect

    // Every row, unfiltered; the delegate filters by `query` as it draws.
    property var allRows: []
    property string query: ""
    // "loading", "ready" or "failed": what the card says while it has no rows.
    property string status: "loading"

    // Replies to an earlier opening are dropped: they describe a binding
    // that may have changed since. Every question one opening asks counts in
    // `pending`, and the rows are built when the last is answered.
    property int generation: 0
    property int pending: 0
    property bool componentsFailed: false
    property var collected: []
    // CardModel.scriptActions' scripts, each to whether KWin has it loaded.
    property var loaded: ({})

    delegate: KeyCard {}

    function open() {
        effect.query = "";
        reload();
        effect.visible = true;
    }

    function close() {
        effect.visible = false;
    }

    function reload() {
        const gen = ++generation;
        const scripts = CardModel.scriptActions.map(s => s.script);
        pending = 1 + scripts.length;
        componentsFailed = false;
        collected = [];
        loaded = {};
        status = "loading";

        ask(gen, "org.kde.kglobalaccel", "/kglobalaccel", "org.kde.KGlobalAccel",
            "allComponents", [], reply => {
                const paths = reply[0] || [];
                effect.pending += paths.length;
                for (const path of paths) {
                    ask(gen, "org.kde.kglobalaccel", String(path), "org.kde.kglobalaccel.Component",
                        "allShortcutInfos", ["default"], infos => {
                            effect.collected = effect.collected.concat(infos[0] || []);
                        }, null);
                }
            }, () => {
                effect.componentsFailed = true;
            });
        for (const script of scripts) {
            ask(gen, "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                "isScriptLoaded", [script], reply => {
                    effect.loaded[script] = reply[0] === true;
                }, null);
        }
    }

    // One asynchronous D-Bus call of opening `gen`. `answered` runs on its
    // reply and `refused` on an error, each only while `gen` is the current
    // opening; either way the call is one fewer pending.
    function ask(gen, service, path, iface, method, args, answered, refused) {
        const call = dbusCall.createObject(effect, {
            service: service,
            path: path,
            dbusInterface: iface,
            method: method,
            arguments: args,
        });
        call.finished.connect(reply => {
            if (gen === effect.generation) {
                answered(reply);
            }
            effect.finishOne(gen);
            call.destroy();
        });
        call.failed.connect(() => {
            if (gen === effect.generation && refused) {
                refused();
            }
            effect.finishOne(gen);
            call.destroy();
        });
        call.call();
    }

    function finishOne(gen) {
        if (gen !== generation) {
            return;
        }
        pending--;
        if (pending > 0) {
            return;
        }
        if (componentsFailed) {
            allRows = [];
            status = "failed";
            return;
        }
        allRows = CardModel.rows(collected, Groups.rules, loaded);
        status = "ready";
    }

    ShortcutHandler {
        name: "KDOS Key Card"
        text: i18nd("kdos-kwin-effect-keys", "Show Key Card")
        sequence: "Meta+F1"
        onActivated: effect.visible ? effect.close() : effect.open()
    }

    Component {
        id: dbusCall

        DBusCall {}
    }
}

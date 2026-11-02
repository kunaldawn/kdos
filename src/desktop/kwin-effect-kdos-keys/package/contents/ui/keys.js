/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

// The key card's model, from what kglobalacceld answers. Pure functions of
// their arguments, so tests/ runs them with no KWin and no session bus.
.pragma library
.import "keynames.js" as KeyNames

// Qt::KeyboardModifier bits of a combined key, in the order QKeySequence
// writes them, so "Meta+Ctrl+Esc" reads as System Settings shows it.
var modifiers = [
    [0x10000000, "Meta"],
    [0x04000000, "Ctrl"],
    [0x08000000, "Alt"],
    [0x02000000, "Shift"],
    [0x20000000, "Num"],
];
var modifierMask = 0x7e000000;
var keyMask = 0x01ffffff;

// One combined key (QKeyCombination::toCombined()) as text. A key with no
// name in Qt's table and outside Unicode is shown by its number, never
// dropped: a chord the card cannot spell is still a chord that is bound.
function keyText(combined) {
    if (!combined) {
        return "";
    }
    var key = combined & keyMask;
    var parts = [];
    for (var i = 0; i < modifiers.length; i++) {
        if (combined & modifiers[i][0]) {
            parts.push(modifiers[i][1]);
        }
    }
    var name = KeyNames.names[key];
    if (name === undefined) {
        if (key === 0x20) {
            name = "Space";
        } else if (key > 0x20 && key < 0x01000000) {
            name = String.fromCodePoint(key);
        } else {
            name = "0x" + key.toString(16);
        }
    }
    parts.push(name);
    return parts.join("+");
}

function match(rule, component, action) {
    return new RegExp(rule[1]).test(component) && new RegExp(rule[2]).test(action);
}

// The group a shortcut is listed under: the first rule of groups.js that
// matches it. The table ends in catch-all rules, so "system" is reached
// only by a table with no fallbacks.
function groupOf(rules, component, action) {
    for (var i = 0; i < rules.length; i++) {
        if (match(rules[i], component, action)) {
            return rules[i][0];
        }
    }
    return "system";
}

function isDesktopEntry(component) {
    return component.length > 8 && component.slice(-8) === ".desktop";
}

// KWin scripts KDOS ships turned off, each with the prefix of the actions it
// registers under the component kwin. kglobalacceld keeps a script's
// actions, keys and all, after the script is unloaded, and reports them
// with no sign that nothing answers them; listed while the script is off,
// they would be keys that do nothing. KWin's /Scripting isScriptLoaded()
// says which are on, and a row of one that is off is left out.
var scriptActions = [
    { script: "krohnkite", component: "kwin", prefix: "Krohnkite" },
];

// The script a shortcut belongs to, from scriptActions, or "" for one that
// belongs to no script there.
function scriptOf(component, action) {
    for (var i = 0; i < scriptActions.length; i++) {
        var s = scriptActions[i];
        if (component === s.component && action.indexOf(s.prefix) === 0) {
            return s.script;
        }
    }
    return "";
}

// One row per shortcut that has a key. `infos` is allShortcutInfos() of
// every component, each entry the unwrapped KGlobalShortcutInfo struct:
// [action, action text, component, component text, context, context text,
// keys, default keys]. A desktop entry's own key is its `_launch` action and
// reads as the application's name; its other actions are prefixed with it,
// so "Capture Rectangular Region" says which program captures. `loaded`
// maps each script of scriptActions to whether KWin has it loaded; a script
// it does not name counts as loaded, so an unanswered question about one
// never hides a key that works.
function rows(infos, rules, loaded) {
    var out = [];
    for (var i = 0; i < infos.length; i++) {
        var info = infos[i];
        if (!info || info.length < 7) {
            continue;
        }
        var keys = [];
        var raw = info[6] || [];
        for (var k = 0; k < raw.length; k++) {
            var t = keyText(raw[k]);
            if (t && keys.indexOf(t) < 0) {
                keys.push(t);
            }
        }
        if (!keys.length) {
            continue;
        }
        var action = String(info[0]);
        var component = String(info[2]);
        var script = scriptOf(component, action);
        if (script && loaded && loaded[script] === false) {
            continue;
        }
        var componentText = String(info[3] || component);
        var text = String(info[1] || action);
        if (isDesktopEntry(component)) {
            text = action === "_launch" ? componentText : componentText + ": " + text;
        }
        out.push({
            group: groupOf(rules, component, action),
            text: text,
            component: component,
            action: action,
            keys: keys,
            haystack: (text + " " + componentText + " " + keys.join(" ")).toLowerCase(),
        });
    }
    out.sort(function (a, b) {
        return a.text.localeCompare(b.text) || a.keys[0].localeCompare(b.keys[0]);
    });
    return out;
}

// A row matches when every word of the query is somewhere in its text, its
// program's name or its keys: "meta shift" lists every Meta+Shift chord.
function matches(row, query) {
    var words = String(query).toLowerCase().split(/\s+/);
    for (var i = 0; i < words.length; i++) {
        if (words[i] && row.haystack.indexOf(words[i]) < 0) {
            return false;
        }
    }
    return true;
}

// The rows that match, in the order of `groups`, one entry per group that
// has any: [{ group, rows }].
function sections(allRows, groups, query) {
    var out = [];
    for (var g = 0; g < groups.length; g++) {
        var list = [];
        for (var i = 0; i < allRows.length; i++) {
            if (allRows[i].group === groups[g] && matches(allRows[i], query)) {
                list.push(allRows[i]);
            }
        }
        if (list.length) {
            out.push({ group: groups[g], rows: list });
        }
    }
    return out;
}

// The sections dealt into `count` columns, each to whichever column is
// shortest so far, a heading counting as two rows. Reading order is kept
// down each column and across them, and no column runs far past the rest,
// which a plain flow of sections does once one group is long.
function columns(allSections, count) {
    var n = Math.max(1, Math.floor(count));
    var out = [];
    var height = [];
    for (var c = 0; c < n; c++) {
        out.push([]);
        height.push(0);
    }
    for (var i = 0; i < allSections.length; i++) {
        var shortest = 0;
        for (var k = 1; k < n; k++) {
            if (height[k] < height[shortest]) {
                shortest = k;
            }
        }
        out[shortest].push(allSections[i]);
        height[shortest] += allSections[i].rows.length + 2;
    }
    return out;
}

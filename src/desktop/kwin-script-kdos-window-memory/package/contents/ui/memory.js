/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

// What window memory remembers and where it puts a window back. Pure
// functions of window-shaped objects and plain rectangles, so tests/ runs
// them with no KWin.
.pragma library

// The most applications remembered. An unbounded history of every program
// ever run is a file nobody can read and a longer scan at every window
// that opens; the cap drops whichever was closed longest ago.
var cap = 200;

// A remembered origin this close to an open window of the same program is
// taken: the second window would open exactly on the first and hide it, so
// KWin's own placement is the better answer.
var originTolerance = 8;

// Programs left alone however they are configured: each places its own
// window, or is a prompt whose place is where it asks from.
var defaultExclude = [
    "org.kde.plasmashell",
    "org.kde.krunner",
    "org.kde.spectacle",
    "org.kde.yakuake",
    "org.kde.polkit-kde-authentication-agent-1",
    "org.kde.ksecretd",
    "org.kde.kwalletd6",
    "org.kde.plasma-welcome",
    "xwaylandvideobridge",
];

// The program a window belongs to: its desktop file name, or its class
// when it has none. A name with white space in it is no name: no
// application id has one, and a window reporting one is not worth trusting
// with another window's place.
function appKey(window) {
    var key = String(window.desktopFileName || window.resourceClass || "");
    return /^[^\s]{1,255}$/.test(key) ? key : "";
}

// Only a program's ordinary top-level window is remembered and put back.
// A transient — a dialog, a file chooser — carries the same application id
// as its parent, so remembering one would make the next document window
// open where the last dialog was.
function eligible(window, exclude) {
    if (!window || window.deleted || !window.managed || !window.normalWindow
            || window.transient || window.specialWindow || window.skipTaskbar
            || !window.moveable) {
        return false;
    }
    var key = appKey(window);
    return key !== "" && exclude.indexOf(key) < 0;
}

// A window whose geometry is its own: not maximized, full screen or in a
// tile. Anything else has the geometry of its state, not of the window.
function isFloating(window) {
    return !window.maximizeMode && !window.fullScreen && !window.tile;
}

function rect(x, y, width, height) {
    return { x: Math.round(x), y: Math.round(y), width: Math.round(width), height: Math.round(height) };
}

function validRecord(r) {
    return r && typeof r.app === "string" && /^[^\s]{1,255}$/.test(r.app)
        && isFinite(r.x) && isFinite(r.y) && isFinite(r.width) && isFinite(r.height)
        && r.width > 0 && r.height > 0
        && (r.max === undefined || (r.max >= 0 && r.max <= 3))
        && (r.desk === undefined || typeof r.desk === "string");
}

// The stored list, most recent first. A record that does not parse is
// absent, and so is a second record for one program: the first is newer.
function parse(text) {
    var list;
    try {
        list = JSON.parse(String(text || "[]"));
    } catch (e) {
        return [];
    }
    if (!Array.isArray(list)) {
        return [];
    }
    var out = [];
    var seen = {};
    for (var i = 0; i < list.length && out.length < cap; i++) {
        var r = list[i];
        if (!validRecord(r) || seen[r.app]) {
            continue;
        }
        seen[r.app] = true;
        out.push({
            app: r.app,
            x: Math.round(r.x),
            y: Math.round(r.y),
            width: Math.round(r.width),
            height: Math.round(r.height),
            max: r.max || 0,
            desk: r.desk || "",
        });
    }
    return out;
}

function serialize(records) {
    return JSON.stringify(records);
}

function find(records, app) {
    for (var i = 0; i < records.length; i++) {
        if (records[i].app === app) {
            return records[i];
        }
    }
    return null;
}

function same(a, b) {
    return a.app === b.app && a.x === b.x && a.y === b.y && a.width === b.width
        && a.height === b.height && a.max === b.max && a.desk === b.desk;
}

// The list with `record` first. `changed` is false when it already was the
// newest record and nothing in it moved, which is the common case of a
// program opened and closed in the same place all day, and costs no write.
function remember(records, record) {
    if (records.length && same(records[0], record)) {
        return { records: records, changed: false };
    }
    var out = [record];
    for (var i = 0; i < records.length && out.length < cap; i++) {
        if (records[i].app !== record.app) {
            out.push(records[i]);
        }
    }
    return { records: out, changed: true };
}

// The screen a remembered rectangle belongs on: the one holding its
// centre, or else the nearest, so a window remembered on a screen that is
// gone still comes back on one that is there. `screens` is a list of
// rectangles; the result is an index into it, or -1 for an empty list.
function nearestScreen(r, screens) {
    var cx = r.x + r.width / 2;
    var cy = r.y + r.height / 2;
    var best = -1;
    var bestDistance = Infinity;
    for (var i = 0; i < screens.length; i++) {
        var s = screens[i];
        var dx = Math.max(s.x - cx, 0, cx - (s.x + s.width));
        var dy = Math.max(s.y - cy, 0, cy - (s.y + s.height));
        var d = dx * dx + dy * dy;
        if (d < bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

// The rectangle moved and shrunk to lie inside `area`, the screen's usable
// area with the panels taken out. A window remembered on a wider screen
// must not open past the edge of a narrower one.
function clamp(r, area) {
    var out = rect(r.x, r.y, Math.min(r.width, area.width), Math.min(r.height, area.height));
    out.x = Math.max(area.x, Math.min(out.x, area.x + area.width - out.width));
    out.y = Math.max(area.y, Math.min(out.y, area.y + area.height - out.height));
    return out;
}

function originTaken(r, others) {
    for (var i = 0; i < others.length; i++) {
        if (Math.abs(others[i].x - r.x) <= originTolerance
                && Math.abs(others[i].y - r.y) <= originTolerance) {
            return true;
        }
    }
    return false;
}

// The comma-separated Exclude setting as a list, the default when unset.
function excludeList(setting) {
    if (setting === undefined || setting === null) {
        return defaultExclude;
    }
    return String(setting).split(",").map(function (s) {
        return s.trim();
    }).filter(function (s) {
        return s.length > 0;
    });
}

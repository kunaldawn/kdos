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

// A window of a program opening this soon after another window of the same
// process closed, or a launch's first window closing this soon after a
// window of its process opened beside it, is a prelude: a splash or a
// first-run notice, whose rectangle is not the program's. The cost is one
// missed update when a process closes a window and opens another inside
// this interval, and one launch left where KWin places it afterwards.
var preludeInterval = 3000;

// A new window whose top-left lies this close to the centre of the work
// area was placed while its size was still a placeholder: centred placement
// put its corner where its middle should be.
var placeholderTolerance = 64;

// How long after opening such a window is centred again when it takes its
// real size. Past it, a resize is the program's or the person's own.
var settleInterval = 2000;

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
// open where the last dialog was. A window that cannot be resized is a
// message box or a fixed notice even when it has no parent, which a Wayland
// client's window always lacks a type to say.
function eligible(window, exclude) {
    if (!window || window.deleted || !window.managed || !window.normalWindow
            || window.transient || window.specialWindow || window.skipTaskbar
            || !window.moveable || !window.resizeable) {
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
        && (r.desk === undefined || typeof r.desk === "string")
        && (r.scale === undefined || (isFinite(r.scale) && r.scale >= 0 && r.scale <= 10))
        && (r.lead === undefined || r.lead === 0 || r.lead === 1);
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
            scale: Number(r.scale) || 0,
            lead: r.lead ? 1 : 0,
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
        && a.height === b.height && a.max === b.max && a.desk === b.desk
        && a.scale === b.scale && (a.lead || 0) === (b.lead || 0);
}

function sameRect(a, b) {
    return !!a && !!b && a.x === b.x && a.y === b.y && a.width === b.width && a.height === b.height;
}

// Two device pixel ratios the same screen could have. 0 is a record made
// before the ratio was kept, and matches any.
function sameScale(a, b) {
    return !a || !b || Math.abs(a - b) < 0.01;
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

// The list without `app`'s record.
function forget(records, app) {
    var out = records.filter(function (r) {
        return r.app !== app;
    });
    return { records: out, changed: out.length !== records.length };
}

// Whether a window of `app` from process `pid` opening at `now` succeeds
// `last`, the window that closed last ({ app, pid, time }), so that the
// record `last` wrote is put back to what it was before.
function prelude(last, app, pid, now) {
    return !!last && last.app === app && pid > 0 && last.pid === pid
        && now >= last.time && now - last.time <= preludeInterval;
}

// The record with `lead` set: its program opens a prelude before the
// window the record is of.
function withLead(record) {
    var out = {};
    for (var k in record) {
        out[k] = record[k];
    }
    out.lead = 1;
    return out;
}

// What opened() does with a new window of a program whose record is
// `record` (or null): "restore" it there; keep the record "pending" for the
// first restore of a window that opened maximized, full screen or tiled;
// leave it where KWin placed it ("lead"), because the record says the
// launch's first window is a splash, and a splash given the program's
// rectangle is stretched across it; or "settle" a window with no record.
// `alone` is no other window of the program being open, `successor` a
// window that prelude() says follows one that just closed. Only "restore"
// moves the window; "lead" and "settle" are both centred again by
// recentre() if KWin placed them at a placeholder size, or fitted into the
// work area by fit() if they open larger than it.
function openAction(record, floating, alone, successor) {
    if (record && record.lead && alone && !successor) {
        return "lead";
    }
    if (record) {
        return floating ? "restore" : "pending";
    }
    return floating ? "settle" : "none";
}

// Whether a launch's first window, closing at `now`, was a prelude to a
// window of its process that opened at `followedAt` (0 for none) while it
// was still open, as a QSplashScreen does when its program calls finish().
// A second window of a program that stays open is not one: its first
// window closes long after it opened, if at all.
function overlapPrelude(followedAt, now) {
    return followedAt > 0 && now >= followedAt && now - followedAt <= preludeInterval;
}

// Whether a window left where KWin placed it as a launch's lead, open from
// `openedAt` to `closedAt`, was the program's own window rather than a
// splash: it stayed open longer than a prelude does. It is one only if no
// successor then opens within preludeInterval, which main.qml waits for.
function leadEnds(openedAt, closedAt) {
    return closedAt - openedAt > preludeInterval;
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
// must not open past the edge of a narrower one; restoreTarget() keeps the
// shrink out of the record.
function clamp(r, area) {
    var out = rect(r.x, r.y, Math.min(r.width, area.width), Math.min(r.height, area.height));
    out.x = Math.max(area.x, Math.min(out.x, area.x + area.width - out.width));
    out.y = Math.max(area.y, Math.min(out.y, area.y + area.height - out.height));
    return out;
}

// Where a window of size `size` on a screen of device pixel ratio `scale`
// goes for `record`, inside `area`. `set` is the rectangle it is given;
// `wanted` is what stays remembered while the window keeps `set`. A record
// made at another ratio gives the place only: the program has picked its
// size for this ratio, and an X11 client handed a size from another one is
// cut off. A clamp into a smaller area is not the window's own size, so it
// is not what `wanted` holds.
function restoreTarget(record, size, scale, area) {
    var wanted = sameScale(record.scale, scale)
        ? rect(record.x, record.y, record.width, record.height)
        : rect(record.x, record.y, size.width, size.height);
    return { set: clamp(wanted, area), wanted: wanted };
}

// What the window's record holds now that its frame is `current`.
// `restored` is restoreTarget()'s answer while it differs from what was
// remembered, plus `reached` once the window has taken `set`: until then
// the window is on its way there, and once there, the first other frame is
// the person's or the program's own size and ends it.
function keepWanted(current, restored) {
    if (!restored) {
        return { rect: current, restored: null };
    }
    if (sameRect(current, restored.set)) {
        restored.reached = true;
        return { rect: restored.wanted, restored: restored };
    }
    if (!restored.reached) {
        return { rect: restored.wanted, restored: restored };
    }
    return { rect: current, restored: null };
}

// A new window placed while its size was a placeholder — a 1x1 X11 window,
// a Wayland surface whose first commit was tiny — keeps that top-left as it
// grows, so it runs off the bottom right. The rectangle centred again in
// `area`, or null when `r` is not one, or has not taken its real size yet.
function recentre(r, area) {
    var cx = area.x + area.width / 2;
    var cy = area.y + area.height / 2;
    if (Math.abs(r.x - cx) > placeholderTolerance || Math.abs(r.y - cy) > placeholderTolerance) {
        return null;
    }
    if (r.width <= 2 * placeholderTolerance && r.height <= 2 * placeholderTolerance) {
        return null;
    }
    return clamp(rect(cx - r.width / 2, cy - r.height / 2, r.width, r.height), area);
}

// A new window that does not fit `area` — one a program sized taller or
// wider than the usable area, which KWin does not shrink — runs past the
// screen edge or under the panel. The rectangle moved and shrunk into
// `area`, or null when `r` already lies inside it.
function fit(r, area) {
    if (r.x >= area.x && r.y >= area.y && r.x + r.width <= area.x + area.width
            && r.y + r.height <= area.y + area.height) {
        return null;
    }
    return clamp(r, area);
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

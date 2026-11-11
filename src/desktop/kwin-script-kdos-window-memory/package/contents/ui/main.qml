/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS window memory — a window opens where its program's last one closed
 *
 * When a program's window closes, its place, its size, whether it was
 * maximized and its screen's device pixel ratio are recorded under the
 * program's application id; when a window of that program opens, it is put
 * back there. On Wayland a program cannot place its own window, so without
 * this every window opens wherever KWin's placement policy puts it, at
 * whatever size the program picked.
 *
 * A window's state is read after the commit that carries it, never inside
 * frameGeometryChanged: KWin emits that before it updates maximizeMode and
 * fullScreen, so a sample taken there would record a maximized frame as
 * the floating one and miss the rectangle a restore returns to.
 *
 * What it will not do, each a way the feature becomes an annoyance:
 *
 * - Change a window that opens maximized, full screen or tiled. That is the
 *   program's or a rule's request, and overriding it is worse than having no
 *   memory; the window's first restore goes to the remembered rectangle,
 *   which KWin otherwise has none of. A window rule that FORCES a position
 *   still wins, because KWin refuses the move; a rule that only sets one
 *   initially is overridden.
 * - Put a window exactly on top of an open window of the same program.
 * - Remember a dialog, a panel, a window that cannot be resized or any other
 *   window that is not a program's ordinary top-level window (memory.js,
 *   eligible()), or a prelude: a window its process closes just before
 *   opening the next, or just after opening the next beside it, such as a
 *   splash or a first-run notice (memory.js, prelude(), overlapPrelude()).
 * - Move a prelude. A record made after one is marked `lead`, and the first
 *   window of the program's next launch is left where KWin places it; the
 *   window after it is restored (memory.js, openAction()). A first window
 *   that stays open longer than a prelude and has no successor was the
 *   program's own, and its close clears the mark (memory.js, leadEnds()).
 * - Remember the shrink that fits a window into a smaller screen, or force
 *   a size remembered at one pixel ratio onto a screen of another
 *   (memory.js, restoreTarget()).
 * - Move a window to another virtual desktop, unless RememberDesktop is set:
 *   a program started from one desktop opens on that desktop, the way the
 *   rest of Plasma behaves.
 *
 * A window with no record that KWin centred while its size was a
 * placeholder is centred again when it takes its real size (memory.js,
 * recentre()), and one that opens larger than the usable area is moved and
 * shrunk into it (memory.js, fit()), so neither runs under the panel.
 *
 * The record lives in $XDG_STATE_HOME/kdos/window-memory, written by QSettings
 * through a temporary file and a rename, so a crash leaves the old file or
 * the new one and never half of one. Settings, in kwinrc
 * [Script-kdos_window_memory]: Exclude (comma-separated application ids) and
 * RememberDesktop (bool).
 * ---------------------------------
 */

import QtQuick
import QtCore
import org.kde.kwin

import "memory.js" as Memory

Item {
    id: root

    readonly property var exclude: Memory.excludeList(KWin.readConfig("Exclude", undefined))
    readonly property bool rememberDesktop: KWin.readConfig("RememberDesktop", false) === true
    property var records: []

    // internalId -> the entry kept for each window being remembered. By the
    // time a window is reported closed it is already marked deleted, so
    // nothing about it is decided then that could be decided when it opened.
    //   app       the program it belongs to;
    //   rect      its last floating rectangle: a maximized, full-screen or
    //             tiled window's frame is its state's, and what is worth
    //             remembering is the rectangle it returns to;
    //   scale     its screen's device pixel ratio when `rect` was taken;
    //   restored  memory.js keepWanted()'s state while the window holds a
    //             rectangle this script shrank to fit;
    //   pending   the record its first restore goes to, for a window that
    //             opened maximized, full screen or tiled;
    //   settle    the time until which a window placed at a placeholder size
    //             is centred again, or one larger than the work area fitted
    //             into it; 0 once done, or once it is moved by hand;
    //   pid, opened  its process and the time it opened;
    //   first     no other window of its program was open when it opened;
    //   followedAt, successor  when a later window of the same process opened
    //             while a first window was open, and that window's entry;
    //   lead      it followed a prelude, so its record keeps `lead`;
    //   left      it was left where KWin placed it as a launch's lead.
    property var tracked: ({})

    // { app, pid, time, previous } of the window that closed last, and the
    // record of its program before that close, for memory.js prelude().
    property var lastClose: null

    // { app, pid, time, record } of a lead window that memory.js leadEnds()
    // says was the program's own: `record` is written, with `lead` cleared,
    // unless a successor opens before leadTimer fires.
    property var leadCommit: null

    Timer {
        id: leadTimer

        interval: Memory.preludeInterval
        onTriggered: root.commitLead()
    }

    Settings {
        id: store

        location: StandardPaths.writableLocation(StandardPaths.GenericStateLocation) + "/kdos/window-memory"
        category: "WindowMemory"
    }

    function save(list) {
        records = list;
        store.setValue("records", Memory.serialize(records));
        store.sync();
    }

    function frame(window) {
        return Memory.rect(window.x, window.y, window.width, window.height);
    }

    function scaleOf(output) {
        return output ? Number(output.devicePixelRatio) || 0 : 0;
    }

    function deskOf(window) {
        return window.desktops.length > 0 ? window.desktops[0] : Workspace.currentDesktop;
    }

    function settle(window, entry) {
        if (!entry.settle) {
            return;
        }
        if (Date.now() > entry.settle || !window.output) {
            entry.settle = 0;
            return;
        }
        const area = Workspace.clientArea(Workspace.MaximizeArea, window.output, deskOf(window));
        const r = Memory.recentre(frame(window), area) || Memory.fit(frame(window), area);
        if (r) {
            entry.settle = 0;
            window.frameGeometry = Qt.rect(r.x, r.y, r.width, r.height);
        }
    }

    function sample(window, entry) {
        if (window.deleted || !Memory.isFloating(window)) {
            return;
        }
        if (entry.pending) {
            const record = entry.pending;
            entry.pending = null;
            place(window, entry, record);
        }
        settle(window, entry);
        const kept = Memory.keepWanted(frame(window), entry.restored);
        entry.rect = kept.rect;
        entry.restored = kept.restored;
        entry.scale = scaleOf(window.output);
    }

    function watch(window, entry) {
        tracked[String(window.internalId)] = entry;
        sample(window, entry);
        const later = () => sample(window, entry);
        const update = () => Qt.callLater(later);
        window.frameGeometryChanged.connect(update);
        window.maximizedChanged.connect(update);
        window.fullScreenChanged.connect(update);
        window.interactiveMoveResizeStarted.connect(() => {
            entry.settle = 0;
            entry.restored = null;
        });
    }

    function siblings(window, key) {
        const out = [];
        const windows = Workspace.windows;
        for (let i = 0; i < windows.length; i++) {
            const other = windows[i];
            if (other !== window && !other.deleted && Memory.appKey(other) === key) {
                out.push(frame(other));
            }
        }
        return out;
    }

    function desktopById(id) {
        for (const desktop of Workspace.desktops) {
            if (desktop.id === id) {
                return desktop;
            }
        }
        return null;
    }

    // Gives a floating window the rectangle `record` asks for; false when
    // there is no screen or the place is taken by a sibling.
    function place(window, entry, record) {
        const screens = Workspace.screens;
        const at = Memory.nearestScreen(record, screens.map(s => s.geometry));
        if (at < 0) {
            return false;
        }
        const area = Workspace.clientArea(Workspace.MaximizeArea, screens[at], deskOf(window));
        const target = Memory.restoreTarget(record, frame(window), scaleOf(screens[at]), area);
        if (Memory.originTaken(target.set, siblings(window, record.app))) {
            return false;
        }
        entry.restored = Memory.sameRect(target.set, record) ? null : { set: target.set, wanted: target.wanted, reached: false };
        window.frameGeometry = Qt.rect(target.set.x, target.set.y, target.set.width, target.set.height);
        return true;
    }

    function restore(window, entry, record) {
        if (rememberDesktop && record.desk && !window.onAllDesktops) {
            const wanted = desktopById(record.desk);
            if (wanted && window.desktops.indexOf(wanted) < 0) {
                window.desktops = [wanted];
                // The window was activated on the desktop it opened on; the
                // view follows it, so the keyboard is never left on a window
                // nobody can see.
                if (window.active) {
                    Workspace.currentDesktop = wanted;
                }
            }
        }

        if (place(window, entry, record) && record.max && window.maximizable) {
            window.setMaximize((record.max & 1) !== 0, (record.max & 2) !== 0);
        }
    }

    function newEntry(window, key, now, first) {
        return {
            app: key, rect: null, scale: 0, restored: null, pending: null, settle: 0,
            pid: Number(window.pid) || 0, opened: now, first: first,
            followedAt: 0, successor: null, lead: false, left: false,
        };
    }

    function commitLead() {
        leadTimer.stop();
        if (!leadCommit) {
            return;
        }
        const result = Memory.remember(records, leadCommit.record);
        leadCommit = null;
        if (result.changed) {
            save(result.records);
        }
    }

    function opened(window) {
        if (!Memory.eligible(window, exclude)) {
            return;
        }
        const key = Memory.appKey(window);
        const pid = Number(window.pid) || 0;
        const now = Date.now();
        const successor = Memory.prelude(lastClose, key, pid, now) || Memory.prelude(leadCommit, key, pid, now);
        if (Memory.prelude(lastClose, key, pid, now)) {
            const back = lastClose.previous ? Memory.remember(records, Memory.withLead(lastClose.previous)) : Memory.forget(records, key);
            if (back.changed) {
                save(back.records);
            }
        }
        lastClose = null;
        if (successor && leadCommit && leadCommit.app === key) {
            leadCommit = null;
            leadTimer.stop();
        }
        const entry = newEntry(window, key, now, siblings(window, key).length === 0);
        entry.lead = successor;
        for (const id in tracked) {
            const other = tracked[id];
            if (other.first && !other.followedAt && other.app === key && pid > 0 && other.pid === pid) {
                other.followedAt = now;
                other.successor = entry;
            }
        }
        const record = Memory.find(records, key);
        const floating = Memory.isFloating(window);
        switch (Memory.openAction(record, floating, entry.first, successor)) {
        case "restore":
            restore(window, entry, record);
            break;
        case "pending":
            entry.pending = record;
            break;
        case "lead":
            entry.left = true;
            entry.settle = floating ? now + Memory.settleInterval : 0;
            break;
        case "settle":
            entry.settle = now + Memory.settleInterval;
            break;
        }
        watch(window, entry);
    }

    function closingRecord(window, entry, previous, lead) {
        const r = entry.rect || previous;
        if (!r) {
            return null;
        }
        return {
            app: entry.app,
            x: r.x,
            y: r.y,
            width: r.width,
            height: r.height,
            max: window.fullScreen ? 0 : Number(window.maximizeMode) || 0,
            desk: window.desktops.length === 1 ? window.desktops[0].id : (previous ? previous.desk : ""),
            scale: entry.rect ? entry.scale : (previous.scale || 0),
            lead: lead,
        };
    }

    function closed(window) {
        const id = String(window.internalId);
        const entry = tracked[id];
        if (!entry) {
            return;
        }
        delete tracked[id];
        const now = Date.now();
        const previous = Memory.find(records, entry.app);
        if (entry.first && Memory.overlapPrelude(entry.followedAt, now)) {
            entry.successor.lead = true;
            if (previous && !previous.lead) {
                save(Memory.remember(records, Memory.withLead(previous)).records);
            }
            return;
        }
        lastClose = { app: entry.app, pid: entry.pid, time: now, previous: previous };
        if (entry.left) {
            const record = Memory.leadEnds(entry.opened, now) ? closingRecord(window, entry, previous, 0) : null;
            if (record) {
                commitLead();
                leadCommit = { app: entry.app, pid: entry.pid, time: now, record: record };
                leadTimer.restart();
            }
            return;
        }
        const record = closingRecord(window, entry, previous, entry.lead || (previous && previous.lead) ? 1 : 0);
        if (!record) {
            return;
        }
        const result = Memory.remember(records, record);
        if (result.changed) {
            save(result.records);
        }
    }

    Connections {
        target: Workspace

        function onWindowAdded(window) {
            root.opened(window);
        }

        function onWindowRemoved(window) {
            root.closed(window);
        }
    }

    Component.onCompleted: {
        records = Memory.parse(store.value("records", "[]"));
        const windows = Workspace.windows;
        for (let i = 0; i < windows.length; i++) {
            if (Memory.eligible(windows[i], exclude)) {
                watch(windows[i], newEntry(windows[i], Memory.appKey(windows[i]), 0, false));
            }
        }
    }
}

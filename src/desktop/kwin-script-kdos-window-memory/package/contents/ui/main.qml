/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS window memory — a window opens where its program's last one closed
 *
 * When a program's window closes, its place, its size and whether it was
 * maximized are recorded under the program's application id; when a window
 * of that program opens, it is put back there. On Wayland a program cannot
 * place its own window, so without this every window opens wherever KWin's
 * placement policy puts it, at whatever size the program picked.
 *
 * What it will not do, each a way the feature becomes an annoyance:
 *
 * - Touch a window that opens maximized, full screen or tiled. That is the
 *   program's or a rule's request, and overriding it is worse than having no
 *   memory. A window rule that FORCES a position still wins, because KWin
 *   refuses the move; a rule that only sets one initially is overridden.
 * - Put a window exactly on top of an open window of the same program.
 * - Remember a dialog, a panel or any other window that is not a program's
 *   ordinary top-level window (memory.js, eligible()).
 * - Move a window to another virtual desktop, unless RememberDesktop is set:
 *   a program started from one desktop opens on that desktop, the way the
 *   rest of Plasma behaves.
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

    // internalId -> { app, rect } for each window being remembered: the
    // program it belongs to, read when it opened, and its last floating
    // rectangle. A maximized, full-screen or tiled window's frame is its
    // state's; what is worth remembering is the rectangle it returns to. By
    // the time a window is reported closed it is already marked deleted, so
    // nothing about it is decided then that could be decided when it opened.
    property var tracked: ({})

    Settings {
        id: store

        location: StandardPaths.writableLocation(StandardPaths.GenericStateLocation) + "/kdos/window-memory"
        category: "WindowMemory"
    }

    function watch(window) {
        const entry = { app: Memory.appKey(window), rect: null };
        const update = () => {
            if (!window.deleted && Memory.isFloating(window)) {
                entry.rect = Memory.rect(window.x, window.y, window.width, window.height);
            }
        };
        update();
        tracked[String(window.internalId)] = entry;
        window.frameGeometryChanged.connect(update);
    }

    function siblings(window, key) {
        const out = [];
        const windows = Workspace.windows;
        for (let i = 0; i < windows.length; i++) {
            const other = windows[i];
            if (other !== window && !other.deleted && Memory.appKey(other) === key) {
                out.push(Memory.rect(other.x, other.y, other.width, other.height));
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

    function restore(window, record) {
        let desktop = window.desktops.length > 0 ? window.desktops[0] : Workspace.currentDesktop;
        if (rememberDesktop && record.desk && !window.onAllDesktops) {
            const wanted = desktopById(record.desk);
            if (wanted && window.desktops.indexOf(wanted) < 0) {
                window.desktops = [wanted];
                desktop = wanted;
                // The window was activated on the desktop it opened on; the
                // view follows it, so the keyboard is never left on a window
                // nobody can see.
                if (window.active) {
                    Workspace.currentDesktop = wanted;
                }
            }
        }

        const screens = Workspace.screens;
        const at = Memory.nearestScreen(record, screens.map(s => s.geometry));
        if (at < 0) {
            return;
        }
        const area = Workspace.clientArea(Workspace.MaximizeArea, screens[at], desktop);
        const wanted = window.resizeable ? record : Memory.rect(record.x, record.y, window.width, window.height);
        const r = Memory.clamp(wanted, area);
        if (Memory.originTaken(r, siblings(window, record.app))) {
            return;
        }
        window.frameGeometry = Qt.rect(r.x, r.y, r.width, r.height);
        if (record.max && window.maximizable) {
            window.setMaximize((record.max & 1) !== 0, (record.max & 2) !== 0);
        }
    }

    function opened(window) {
        if (!Memory.eligible(window, exclude)) {
            return;
        }
        const record = Memory.find(records, Memory.appKey(window));
        if (record && Memory.isFloating(window)) {
            restore(window, record);
        }
        watch(window);
    }

    function closed(window) {
        const id = String(window.internalId);
        const entry = tracked[id];
        if (!entry) {
            return;
        }
        delete tracked[id];
        const previous = Memory.find(records, entry.app);
        const r = entry.rect || previous;
        if (!r) {
            return;
        }
        const desk = window.desktops.length === 1 ? window.desktops[0].id : (previous ? previous.desk : "");
        const result = Memory.remember(records, {
            app: entry.app,
            x: r.x,
            y: r.y,
            width: r.width,
            height: r.height,
            max: window.fullScreen ? 0 : Number(window.maximizeMode) || 0,
            desk: desk,
        });
        if (result.changed) {
            records = result.records;
            store.setValue("records", Memory.serialize(records));
            store.sync();
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
                watch(windows[i]);
            }
        }
    }
}

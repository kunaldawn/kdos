/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS run-or-raise — one key per terminal program
 *
 * A key raises the program's window when one is open and opens one when
 * none is. Which window is the program's is raise.js's rule.
 *
 * A KWin SCRIPT CANNOT START A PROGRAM, so opening goes through
 * kglobalacceld: each program has a desktop entry in /usr/share/kglobalaccel,
 * which kglobalacceld loads as a component, and invoking its `_launch`
 * action runs the entry's Exec line through KIO. The entry has no key of its
 * own: the key is the ShortcutHandler's below, and a key has one owner.
 *
 * Every program here has a desktop entry named org.kdos.runraise.<program>
 * .desktop beside this package; CMake refuses a build where the two lists
 * differ. Each ShortcutHandler is written out, not generated from a list,
 * so testing/chordcheck.py and kdos-keycard-gen --check can read its key.
 * ---------------------------------
 */

import QtQuick
import org.kde.kwin

import "raise.js" as Raise

Item {
    id: root

    // A press opens one window: a second press before that window maps
    // would open another. Each program is held here until a window of its
    // opens or the hold runs out, which is the case where the launch failed.
    readonly property int launchHoldMs: 4000
    property var held: ({})

    function raiseOrRun(program) {
        const found = Raise.matching(Workspace.windows, program);
        const target = Raise.pick(found, Workspace.activeWindow);
        if (target) {
            bringForward(target);
            return;
        }
        if (found.length > 0) {
            return;
        }
        const now = Date.now();
        if ((held[program] || 0) > now) {
            return;
        }
        held[program] = now + launchHoldMs;
        open(program);
    }

    function bringForward(window) {
        if (window.minimized) {
            window.minimized = false;
        }
        if (!window.onAllDesktops && window.desktops.length > 0
                && window.desktops.indexOf(Workspace.currentDesktop) < 0) {
            Workspace.currentDesktop = window.desktops[0];
        }
        Workspace.activeWindow = window;
    }

    function open(program) {
        const component = Raise.launcherComponent(program);
        const call = launcher.createObject(root, { path: Raise.componentPath(component) });
        call.finished.connect(() => call.destroy());
        call.failed.connect(() => {
            console.warn("kdos_runraise: kglobalacceld has no component " + component);
            delete root.held[program];
            call.destroy();
        });
        call.call();
    }

    Connections {
        target: Workspace

        function onWindowAdded(window) {
            for (const program in root.held) {
                if (Raise.isProgramWindow(window, program)) {
                    delete root.held[program];
                }
            }
        }
    }

    Component {
        id: launcher

        DBusCall {
            service: "org.kde.kglobalaccel"
            dbusInterface: "org.kde.kglobalaccel.Component"
            method: "invokeShortcut"
            arguments: ["_launch", "default"]
        }
    }

    ShortcutHandler {
        name: "KDOS Raise mc"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open Midnight Commander")
        sequence: "Meta+Shift+M"
        onActivated: root.raiseOrRun("mc")
    }

    ShortcutHandler {
        name: "KDOS Raise aerc"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open aerc (mail)")
        sequence: "Meta+Shift+E"
        onActivated: root.raiseOrRun("aerc")
    }

    ShortcutHandler {
        name: "KDOS Raise lynx"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open Lynx (web)")
        sequence: "Meta+Shift+B"
        onActivated: root.raiseOrRun("lynx")
    }

    ShortcutHandler {
        name: "KDOS Raise rmpc"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open rmpc (music)")
        sequence: "Meta+Shift+U"
        onActivated: root.raiseOrRun("rmpc")
    }

    ShortcutHandler {
        name: "KDOS Raise ikhal"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open ikhal (calendar)")
        sequence: "Meta+Shift+C"
        onActivated: root.raiseOrRun("ikhal")
    }

    ShortcutHandler {
        name: "KDOS Raise iamb"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open iamb (chat)")
        sequence: "Meta+Shift+G"
        onActivated: root.raiseOrRun("iamb")
    }

    ShortcutHandler {
        name: "KDOS Raise micro"
        text: i18nd("kdos-kwin-script-runraise", "Raise or Open micro (editor)")
        sequence: "Meta+Shift+W"
        onActivated: root.raiseOrRun("micro")
    }
}

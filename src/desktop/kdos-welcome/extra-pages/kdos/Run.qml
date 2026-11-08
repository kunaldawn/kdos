/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Run.qml — every command a KDOS page runs
 *
 * A page may only run programs through Welcome Center's Utils.runCommand,
 * which takes ONE string, splits it on single spaces and starts the first
 * word as the program with the rest as its arguments: there is no shell, so
 * no quoting either. That makes the argument vector the contract here. A
 * page hands over a list; an argument that is empty or holds whitespace would
 * come out the other side as different arguments, so it is refused and the
 * command is not run at all. Every value a page passes is a fixed word or a
 * name KDOS printed, never text a person typed.
 *
 *   run(argv, done)    done(exitCode, text): stdout on success, the error
 *                      Welcome Center composed otherwise; -1 when the
 *                      program is not installed, -2 when refused here
 *   json(argv, done)   the same, with stdout parsed; done(exitCode, value)
 *                      with value null when it is not JSON
 *   config(file, group, key, fallback, done)
 *                      kreadconfig6 of one key, done(value as text)
 * ---------------------------------
 */

import QtQml

import org.kde.plasma.welcome as Welcome

QtObject {
    id: runner

    // How many commands this object has started and not heard back from;
    // a page greys its controls while it is above zero.
    property int pending: 0
    readonly property bool busy: pending > 0

    function safe(argv: list<string>): bool {
        if (argv.length === 0) {
            return false;
        }
        for (const a of argv) {
            if (a.length === 0 || /\s/.test(a)) {
                return false;
            }
        }
        return true;
    }

    function run(argv: list<string>, done: var): void {
        if (!safe(argv)) {
            console.warn("kdos-welcome: refused to run", JSON.stringify(argv));
            if (done) {
                done(-2, i18nd("kdos-welcome", "An argument held a space, so the command was not run."));
            }
            return;
        }
        pending++;
        Welcome.Utils.runCommand(argv.join(" "), (code, text) => {
            pending--;
            if (done) {
                done(code, text);
            }
        });
    }

    function json(argv: list<string>, done: var): void {
        run(argv, (code, text) => {
            let value = null;
            if (code === 0) {
                try {
                    value = JSON.parse(text);
                } catch (e) {
                    value = null;
                }
            }
            done(code, value);
        });
    }

    function config(file: string, group: string, key: string, fallback: string, done: var): void {
        const argv = ["kreadconfig6", "--file", file, "--group", group, "--key", key];
        if (fallback.length > 0) {
            argv.push("--default", fallback);
        }
        run(argv, (code, text) => done(code === 0 ? text.trim() : fallback));
    }

    function writeConfig(file: string, group: string, key: string, value: string, done: var): void {
        run(["kwriteconfig6", "--file", file, "--group", group, "--key", key, "--notify", value], done);
    }

    function deleteConfig(file: string, group: string, key: string, done: var): void {
        run(["kwriteconfig6", "--file", file, "--group", group, "--key", key, "--notify", "--delete"], done);
    }

    // A D-Bus method on the session bus that returns one boolean, through
    // dbus-send, the way KDE's own desktop files call them.
    function dbusBool(dest: string, path: string, method: string, args: list<string>, done: var): void {
        const argv = ["dbus-send", "--session", "--print-reply", "--dest=" + dest, path, method].concat(args);
        run(argv, (code, text) => done(code === 0 && /boolean\s+true/.test(text)));
    }

    function dbusCall(dest: string, path: string, method: string, args: list<string>, done: var): void {
        run(["dbus-send", "--session", "--print-reply", "--dest=" + dest, path, method].concat(args), done);
    }
}

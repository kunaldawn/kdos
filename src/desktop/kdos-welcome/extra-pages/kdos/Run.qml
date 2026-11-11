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
 *                      program is not installed, -2 when refused here.
 *                      Counted in `pending` until the program exits, so it
 *                      is for commands that finish: a page greys its
 *                      controls while one runs
 *   read(argv, done)   the same, not counted: for a command that only
 *                      reads, so re-reading a page never greys it
 *   json(argv, done)   run(), with stdout parsed; done(exitCode, value)
 *                      with value null when it is not JSON
 *   readJson(argv, done)
 *                      the same through read()
 *   launch(argv, done) starts a program that stays open (a settings page,
 *                      Discover, the Help Center), not counted; done(code,
 *                      text) is called only when it is missing, refused or
 *                      exits with an error
 *   config(file, group, key, fallback, done)
 *                      kreadconfig6 of one key through read(), done(value
 *                      as text)
 *
 * A failure's text is one sentence. Welcome Center's ends with everything
 * the command wrote to its error output, which for a graphical program is
 * a page of driver and font warnings, so it is cut to the sentence up to
 * "failed: " and the command's last line of output.
 * ---------------------------------
 */

import QtQml

import org.kde.plasma.welcome as Welcome

QtObject {
    id: runner

    // How many run() and json() commands this object has started and not
    // heard back from; a page greys its controls while it is above zero.
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

    function brief(text: string): string {
        const lines = text.split(/\n|<br\s*\/?>/).map(l => l.trim()).filter(l => l.length > 0);
        if (lines.length <= 1) {
            return text.trim();
        }
        const cut = lines[0].indexOf("failed: ");
        if (cut < 0) {
            return lines[0];
        }
        return lines[0].slice(0, cut + 8) + lines[lines.length - 1];
    }

    function start(argv: list<string>, counted: bool, done: var): void {
        if (!safe(argv)) {
            console.warn("kdos-welcome: refused to run", JSON.stringify(argv));
            if (done) {
                done(-2, i18nd("kdos-welcome", "An argument held a space, so the command was not run."));
            }
            return;
        }
        if (counted) {
            pending++;
        }
        Welcome.Utils.runCommand(argv.join(" "), (code, text) => {
            if (counted) {
                pending--;
            }
            if (done) {
                done(code, code === 0 ? text : brief(text));
            }
        });
    }

    function run(argv: list<string>, done: var): void {
        start(argv, true, done);
    }

    function read(argv: list<string>, done: var): void {
        start(argv, false, done);
    }

    function launch(argv: list<string>, done: var): void {
        start(argv, false, (code, text) => {
            if (code !== 0 && done) {
                done(code, text);
            }
        });
    }

    function parsed(code: int, text: string): var {
        if (code !== 0) {
            return null;
        }
        try {
            return JSON.parse(text);
        } catch (e) {
            return null;
        }
    }

    function json(argv: list<string>, done: var): void {
        run(argv, (code, text) => done(code, parsed(code, text)));
    }

    function readJson(argv: list<string>, done: var): void {
        read(argv, (code, text) => done(code, parsed(code, text)));
    }

    function config(file: string, group: string, key: string, fallback: string, done: var): void {
        const argv = ["kreadconfig6", "--file", file, "--group", group, "--key", key];
        if (fallback.length > 0) {
            argv.push("--default", fallback);
        }
        read(argv, (code, text) => done(code === 0 ? text.trim() : fallback));
    }

    function writeConfig(file: string, group: string, key: string, value: string, done: var): void {
        run(["kwriteconfig6", "--file", file, "--group", group, "--key", key, "--notify", value], done);
    }

    function deleteConfig(file: string, group: string, key: string, done: var): void {
        run(["kwriteconfig6", "--file", file, "--group", group, "--key", key, "--notify", "--delete"], done);
    }

    // A D-Bus method on the session bus that returns one boolean, through
    // dbus-send, the way KDE's own desktop files call them; a read, so not
    // counted.
    function dbusBool(dest: string, path: string, method: string, args: list<string>, done: var): void {
        const argv = ["dbus-send", "--session", "--print-reply", "--dest=" + dest, path, method].concat(args);
        read(argv, (code, text) => done(code === 0 && /boolean\s+true/.test(text)));
    }

    function dbusCall(dest: string, path: string, method: string, args: list<string>, done: var): void {
        run(["dbus-send", "--session", "--print-reply", "--dest=" + dest, path, method].concat(args), done);
    }
}

/*
    SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
    SPDX-License-Identifier: MIT
*/

// Which window a run-or-raise key means. Pure functions of window-shaped
// objects, so tests/ runs them with no KWin.
.pragma library

// Konsole gives every window of every process one application id,
// org.kde.konsole, so a terminal program's window is told apart by its
// caption. The caption is the active tab's title with " — Konsole" after it
// (Qt appends the application's display name), and the tab title is
//
//   <program>          when the launcher started it: `-p tabtitle=<program>`;
//   <dir> : <program>  when anything else did, Konsole's own "%d : %n",
//                      where %n is the foreground process's name.
//
// The window title a program sets with an escape sequence is not in either:
// Konsole shows it in the caption only when "Show window title on the title
// bar" is on, and a person who turns that on loses the second form.
var terminalIds = ["org.kde.konsole", "konsole"];

function isTerminal(window) {
    return terminalIds.indexOf(String(window.desktopFileName)) >= 0
        || terminalIds.indexOf(String(window.resourceClass)) >= 0;
}

// The tab title: the caption without the " — <application>" Qt appended,
// which is always an em dash between two spaces.
function tabTitle(caption) {
    return String(caption).replace(/ — [^—]*$/, "");
}

// A window is the program's when it is the program's own window (its
// application id is the program's name) or a terminal whose active tab runs
// it. Dialogs, panels and other special windows never are.
function isProgramWindow(window, program) {
    if (!window || !window.normalWindow || window.deleted) {
        return false;
    }
    if (window.desktopFileName === program || window.resourceClass === program) {
        return true;
    }
    if (!isTerminal(window)) {
        return false;
    }
    var title = tabTitle(window.caption);
    return title === program || title.slice(-(program.length + 3)) === " : " + program;
}

function matching(windows, program) {
    var out = [];
    for (var i = 0; i < windows.length; i++) {
        if (isProgramWindow(windows[i], program)) {
            out.push(windows[i]);
        }
    }
    return out;
}

// The window a press brings forward, or null when there is nothing to raise.
// From another window it is the program's window used last (the top of the
// stacking order); from one of the program's own windows it is the next of
// them, so repeated presses walk every window the program has open. A
// program's only window, already active, stays as it is.
function pick(found, active) {
    if (!found.length) {
        return null;
    }
    var at = found.indexOf(active);
    if (at >= 0) {
        return found.length > 1 ? found[(at + 1) % found.length] : null;
    }
    var best = found[0];
    for (var i = 1; i < found.length; i++) {
        if (found[i].stackingOrder > best.stackingOrder) {
            best = found[i];
        }
    }
    return best;
}

// The desktop entry that opens a new window for the program: kglobalacceld
// loads it from /usr/share/kglobalaccel as the component of that name, and
// its `_launch` action runs its Exec line.
function launcherComponent(program) {
    return "org.kdos.runraise." + program + ".desktop";
}

// kglobalacceld's object path for a component: every character outside
// [A-Za-z0-9_] becomes "_", as Component::dbusPath() writes it.
function componentPath(component) {
    return "/component/" + String(component).replace(/[^A-Za-z0-9_]/g, "_");
}

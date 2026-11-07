/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   A new window opens on the screen the pointer is on.
 *
 * With several screens, a program started from a launcher on one of them may
 * open its window on another, where it remembered being or where KWin's
 * placement put it. Every new normal window, and every new dialog without a
 * parent, is sent to the screen under the pointer when it appears; a dialog
 * that belongs to a window, a window that is already there, a fullscreen
 * one and a window KWin will not move are left where they are.
 *
 * SPDX-License-Identifier: MIT
 * ---------------------------------
 */

function place(window) {
    if (!window || window.deleted || window.specialWindow || !window.moveable) {
        return;
    }
    if (!(window.normalWindow || window.dialog) || window.transient || window.fullScreen) {
        return;
    }
    const target = workspace.screenAt(workspace.cursorPos);
    if (target && window.output !== target) {
        workspace.sendClientToScreen(window, target);
    }
}

workspace.windowAdded.connect(place);

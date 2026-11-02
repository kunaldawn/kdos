/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Picture-in-picture windows stay on top, on every desktop.
 *
 * Firefox, Chromium and their forks open a picture-in-picture video as an
 * ordinary toplevel titled "Picture-in-Picture" (Chromium: "Picture in
 * picture"), which the next click on any other window buries. A window is
 * recognised by that title alone, and the title can arrive after the window
 * is mapped, so a window is checked when it appears and again whenever its
 * caption changes; once pinned it is left alone, so a person who unpins it
 * from the window menu keeps it unpinned.
 *
 * SPDX-License-Identifier: MIT
 * ---------------------------------
 */

const PIP_TITLE = /^picture[- ]in[- ]picture$/i;
const pinned = new Set();

function pin(window) {
    if (!window || window.deleted || window.specialWindow || pinned.has(window)) {
        return;
    }
    if (!PIP_TITLE.test(window.caption.trim())) {
        return;
    }
    pinned.add(window);
    window.keepAbove = true;
    window.onAllDesktops = true;
}

function watch(window) {
    if (!window || window.specialWindow) {
        return;
    }
    pin(window);
    window.captionChanged.connect(() => pin(window));
    window.closed.connect(() => pinned.delete(window));
}

workspace.windowList().forEach(watch);
workspace.windowAdded.connect(watch);

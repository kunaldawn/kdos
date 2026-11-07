# The desktop, read by a person

`testing/usability.sh` drives a booted ISO through the things a hand does in the
first two minutes of a session and leaves a numbered contact sheet in
`build/shots/usability/`. This page is what to read it against: one heading per
shot, and under each the questions that have an answer in the picture.

**It asserts nothing on purpose.** The defects this sweep exists to catch pass
`preflight.sh`, `selftest.sh` and `plasmatest.sh`: those prove the wiring and
the decisions, and none of them has ever hovered a button. What is automated
here is the driving; the pass is a person looking.

**The shots are raw PPM despite the `.png`.** `convert 04-start-menu.png real.png`
before opening one.

```sh
testing/rig-image.sh                 # once, if kdos-qemu-py:latest is not built
testing/usability.sh                 # 1280x800
testing/usability.sh 1600x900        # another size, another panel length
```

The run starts when the rig's readiness gate passes: the live session's
Plasma shell is up and the splash is gone. The script aims at the panel from
the desk's layout, a bottom panel whose centre line is twenty pixels above the
screen's bottom edge; a run at another font size or scale has a different
panel, and the numbers come back out of a fresh 02 shot.

---

## 01 — Welcome Center

Welcome Center opens on the first login of the live session, with the KDOS
pages after Plasma's own.

- Is the window inside the screen, framed and titled?
- Does the first page show the KDOS mark and its one sentence, with no text cut?
- Are the panel and the desktop visible around it rather than under it?

## 02 — the desktop, Welcome Center closed

The reference frame for everything below.

- **Is the panel ON the bottom edge, opaque, and the full width of the
  screen?** A strip of desktop under it means the panel is floating or was laid
  out by another desk.
- Left to right: Start (the mark and the word), the pager, the pinned
  Konsole, Dolphin, Firefox and Kate, the meters, the tray, the clock with the
  date, Show Desktop.
- Is the clock complete, in the fixed-width face, not clipped at the right?

## 03 — the pointer on `Start`

- **Does the tooltip clear the button it is describing?**
- Is the word `Start` still legible on its hover highlight?
- Does the panel still show everything it showed in 02?

## 04 — Kickoff, opened by a click on `Start`

- **Did it open at all?** A tooltip that took the click opens nothing.
- **Does it sit above the panel rather than across it?**
- Is its bottom-left corner against the Start button it belongs to?
- Are the favourites the browser, Konsole, Dolphin, System Settings and
  Discover, in that order?

## 05 — the pointer moved into the menu

- **Is the menu still open?**
- **Is the row under the pointer highlighted, and only that row?**

## 06 — the pointer back over the panel, menu open

- **Is the menu still open?** A tooltip raised over a panel item must not
  close the popup under it.
- Is the Start button drawn as pressed, and still readable?

## 07 — `Esc`

- Is the menu gone?
- **Is the Start button back to its resting look?** Hover state that outlives
  the pointer means no leave was reported.

## 08 — the pointer on the clock

- Does the tip describe the clock, clear of it?
- Is the clock itself unchanged underneath?

## 09 — the calendar

- Did it open, above the panel, near the clock?
- Is the month grid complete, with today marked?

## 10 — Konsole, `Meta+Return`

- Did a window open, framed and titled, with the KDOS decoration?
- **Does it stop above the panel?**
- Is there a task entry for it, and does the pinned Konsole show it running
  rather than a second icon?

## 11 — the panel with a window open

- Is the running Konsole marked on its pinned icon?
- **Is there a gap between the groups?** The Start button, the pinned apps and
  the meters with the tray read as one run without the separator.
- Do the meters read plausibly: a processor and a memory figure, a network rate?

## 12 — the desktop menu, a right click on bare desktop

- Did it open at the pointer, fully on screen?
- Are its entries the desktop's (Add Widgets, Configure Desktop and Wallpaper,
  and the rest), with none duplicated?

## 13 — search, `Meta+Space`

- Is KRunner at the top of the screen with the caret in its field?
- Are the results grouped and headed once something is typed?

## 14 — a notification

**Konsole is clicked first, and that click is load-bearing.** The desktop
menu and the search took the focus, and a `--type` without it reaches nothing,
so this shot would be a desktop with nothing on it.

**The shot comes straight after the notification, with no sleep.** A toast
lives five seconds and a rig `--shot` costs a good part of that on its own, so
a step that waits first photographs the desktop the toast has already left.

- Is the toast in the bottom right, above the panel, inside the screen?
- **Did Konsole keep the keyboard?** A toast asks for none; one that took the
  focus would leave the next keystroke going nowhere.

## 15 — the toast expired

- Is it gone, and is what it covered drawn again?
- Is the notification in the tray's history?

---

## What a failure here looks like in the tree

| What the shot shows | Where it lives |
|---|---|
| A strip of desktop under the panel; a floating or translucent panel | the desk's layout script, `src/desktop/kdos-plasma-look/desks/org.kdos.desk.kdos/contents/layout.js`, and whether the first login ran it |
| Another panel order, or a pinned app missing | the same script, and `fs/etc/skel/.config/kdos/favorites` (`testing/lnfcheck.py` compares the two) |
| A popup across the panel; a window under it | the panel's strut, which KWin reads from plasmashell |
| A tip over its own button; a click that opens nothing | the applet's tooltip area in plasmashell |
| A colour or face that is not the accent's | the global theme and colour scheme, `src/desktop/kdos-plasma-look`, and `testing/defaultscheck.py`'s rows |
| A toast that takes the keyboard | the notification's window type in plasmashell, and KWin's focus rules |

Related: [`testing.md`](../docs/kdos/05-developer/testing.md) for the harnesses
this one is built on, and
[`kwin.md`](../docs/kdos/04-programs/kwin.md) for the compositor these checks
run under.

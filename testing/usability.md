# The console desktop, read by a person

`testing/usability.sh` drives a booted ISO through the things a hand does in the
first two minutes of a session and leaves a numbered contact sheet in
`build/shots/usability/`. This page is what to read it against: one heading per
shot, and under each the questions that have an answer in the picture.

**It asserts nothing on purpose.** Every defect this sweep exists to catch was
green in `preflight.sh` and green in `selftest.sh` on the day it shipped — they
prove the wiring and the decisions, and neither of them has ever hovered a
button. What is automated here is the driving; the pass is a person looking.

**The shots are raw PPM despite the `.png`.** `convert 04-start-menu.png real.png`
before opening one.

```sh
testing/rig-image.sh                 # once, if kdos-qemu-py:latest is not built
testing/usability.sh                 # 1280x800
testing/usability.sh 1600x900        # a different cell count
```

A run is about six minutes: four of boot, two of steps.

---

## 01 — the welcome card

The card is open on first login and is the first thing anyone sees.

- Is it inside the screen, with its border closed on all four sides?
- Does its own text fit — no row cut mid-word, no row past the right border?
- Are the desktop icons and the taskbar visible around it rather than under it?

## 02 — the desktop, card dismissed

The reference frame for everything below.

- **Is the taskbar ON the bottom edge?** A strip of desktop under the bar means
  a docked surface has been moved out of its own exclusive zone — the defect
  that also puts every menu and tooltip on top of the bar.
- **Does the bar have a visible boundary against the desktop?** `KT_SURFACE` is
  one shade off `KT_BG`; on a display with no pixel layer the bar's edge is a
  row of its own, in the double horizontal.
- Are the desktop icons at the top left, each with its label?
- **Is the desktop's hint row at the bottom of the screen?** Floating in the
  middle of it means the icon layer is not the size of the desktop.
- Is the clock complete at the right edge — not clipped by the last column?

## 03 — the pointer on `Start`

- **Does the tooltip clear the button it is describing?** A tip drawn over its
  own subject is the one thing a tip may not do.
- **Is the word `Start` still legible?** The button lights on hover, and its ink
  is read against the plate; a button that goes blank under the pointer is a
  plate that was never drawn.
- Does the bar still show everything it showed in 02?

## 04 — the menu, opened by a click on `Start`

- **Did it open at all?** Nothing is the failure a tooltip used to cause: the
  tip sat over the button and took the click.
- **Does it sit above the bar rather than across it?**
- Is its bottom-left corner against the Start button it belongs to?
- Are all three columns drawn, with their headings and their rules?

## 05 — the pointer moved into the menu

- **Is the menu still open?** It closes when it loses the keyboard, so anything
  that takes focus while the pointer crosses the screen closes it.
- **Is the row under the pointer highlighted, and only that row?** The highlight
  is a plate under a compositor and an accent fill on the console; a menu whose
  hint line names a row that nothing on screen marks is the plate being drawn
  into a layer this display has not got.

## 06 — the pointer back over the taskbar, menu open

- **Is the menu still open?** This is where a tooltip is raised over a bar
  button, and where the menu used to vanish.
- Is the `Start` button drawn as opened — and still readable?

## 07 — `Esc`

- Is the menu gone?
- **Is the `Start` button back to its resting look, with its word?** Hover state
  that outlives the pointer means no leave was reported.

## 08 — the pointer on the clock

- Does the tip describe the clock, clear of it?
- Is the clock itself unchanged underneath?

## 09 — the calendar

- Did it open, above the bar, near the clock?
- Is the month grid complete, with today marked?

## 10 — a terminal, `Super+Return`

- Did a window open, framed and titled?
- **Does it stop above the taskbar?**
- Is there a taskbar entry for it, with its state marker?

## 11 — the bar with a window open

- **Is the window button drawn as a button — a filled tile with a name, not a
  bare word and not a lone letter?** Icon mode is a 40x40 square whose shape and
  state are pixels, so a cell display gets the labelled chip instead.
- **Does the chip say whether the window is minimised?** The underline is
  pixels; on the console it is the marker cell.
- **Is there a separator between the segments?** The Start button, the window
  list and the status wing are three groups and read as one run without them.
- Do the meters read plausibly — a percentage, a chart, a rate?

## 12 — the root menu, `Super+F10`

- Is it the same menu as 04, centred rather than cornered?
- Is it complete — no column cut by the screen edge?

## 13 — search, `Super+Space`

- Is the field at the top with the caret in it?
- Are the results grouped and headed?
- Is the selected row highlighted across its whole width?

## 14 — a click on bare desktop

- **Did anything answer it?** The icon layer is under every window and takes the
  clicks no window claimed; if it is not the size of the desktop, a click in the
  lower right reaches nothing at all.
- Did the icon selection clear?

## 15 — the pointer low on the desktop

- Is the pointer cell drawn where the pointer is?
- Nothing else should have changed.

## 16 — a notification

**The terminal is clicked first, and that click is load-bearing.** Shots 14 and
15 leave the focus on the icon layer, and a `--type` there reaches the icon
layer's own type-ahead rather than a shell — so without it this shot is a bare
desktop, every run, and says nothing about notifications at all.

**The shot comes straight after the notification, with no sleep**, and that is
not tidiness: a toast lives five seconds and a rig `--shot` costs a good part of
that on its own, so a step that waits first photographs the desktop the toast
has already left. Measured — `date` either side of four shots put them sixty-
eight seconds apart.

- Is the toast in the top right, inside the screen?
- **Did the terminal keep the keyboard?** A toast asks for none; one that took
  the focus would leave the next keystroke going nowhere, and would close any
  menu that happened to be open.
- The toast is drawn over the desktop: a click where it is should reach what is
  under it, not the toast.

## 17 — the toast expired

- Is it gone, and is what it covered drawn again?

---

## What a failure here looks like in the tree

| What the shot shows | Where it lives |
|---|---|
| A strip of desktop under the bar; menus on top of the bar | the work-area walk in `kdos-con` — one walk, and it skips panels |
| A tip over its own button; a click that opens nothing | the surface's input region, `kdisp_input_cells()` |
| A menu that closes when a tip appears | whether an overlay that asked for no keyboard is focused |
| A button that stays lit after the pointer leaves | the pointer leave the display reports as `(-1,-1)` |
| A blank button under the pointer | a plate drawn only in pixels, on a display that has none |
| A menu row selected by the hint line and by nothing visible | `kch_px_live()` — a plate is the whole highlight only where a backdrop replays it |
| A window button that is one letter in a square | icon mode chosen on a display with no plate and no underline |
| Icons and hints stranded in a corner | the icon layer's size — it is the session's, never the client's |

Related: [`testing.md`](../docs/kdos/05-developer/testing.md) for the harnesses
this one is built on, and
[`kdos-con.md`](../docs/kdos/04-programs/kdos-con.md) for the roles and the
rules each of these checks.

# KDOS Mobile Desktop — Touchscreen Shell, On-Screen Keyboard and Convergence

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or
> `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`)
> syntax for tracking.
>
> **This plan does NOT contain commit steps.** KDOS hard rule 5 — *"Do not auto-commit. The user
> commits manually, often squashing many edits into one logical commit."* — overrides the usual
> commit-per-task convention. Each task ends with a **Checkpoint**: a command whose output proves
> the task landed. Stop there and let the user commit.

**Goal:** Give KDOS a desktop for a touchscreen. A mobile-only package (`kdos-mobile`) and an
on-screen keyboard package (`kdos-osk`) that keep every aesthetic and every principle of the
desktop — the character-cell grid, the phosphor palette, the eight colours, one binary under many
names, no GTK and no Qt on the host — and are redesigned from nothing for a finger on a 6.41"
screen. Plus the thing only this distro can offer: **the same phone drives an external monitor
with the DESKTOP shell at the same time**, so a smartphone running KDOS is a complete
touchscreen workstation.

**Architecture:** `wl_touch` is bound in libkwl and disambiguated there — tap, drag, long-press,
kinetic scroll and pinch — and re-emitted as the ordinary mouse events every libktui widget
already handles, so the whole existing toolkit inherits touch without being rewritten. System
gestures are recognised in the compositor (`kdos-gesture.c`) because only the compositor can take
a touch away from a client. `kdos-osk` is a layer-shell surface with an exclusive zone, so
windows are genuinely resized rather than covered. `kdos-mobile` is one binary under seven names,
drawn by libktui exactly as `kdos-shell` is. `kdos-mode.c` classifies each output and each seat
and picks a chrome template set **per output**, which is what makes convergence nearly free:
`kdos-child.c` already spawns chrome per output.

**Tech Stack:** libkwl + libktui + libkcell + libkicon + libkchrome + libkcolor + libkbase +
libkxdg + libkproc; wayland-client, `zwp_virtual_keyboard_v1`, `zwp_input_method_v2`,
`zwlr_layer_shell_v1` v4, `wlr-foreign-toplevel`, `ext-workspace`; the labwc fork
(`kdos-comp`) with six new grafts; mesa/freedreno and wlroots on aarch64.

**Spec:** This document.

---

## Relationship to `mobile.plan.md`

`mobile.plan.md` is the **hardware arc**: `aarch64-kdos-linux-musl`, the OnePlus 6T (`fajita`,
SDM845) and QEMU `-M virt`, a sibling phase tree (`script-mobile/`) and build root
(`build-mobile/`), reaching a login prompt over USB ethernet and SSH. Its *"What is explicitly
NOT in this arc"* section names the boundary between the two plans:

> No desktop: no `kdos-comp`, `kdos-shell`, wlroots, mesa, Wayland, display or touch.
> No appbox / distrobox / podman. No Wi-Fi.

**This plan is everything on the other side of that line.** It assumes `mobile.plan.md` has
landed: a bootable aarch64 rootfs on both boards, `kdosbuild` driving `script-mobile/`,
`PORT_REPO` putting `ports/mobile` ahead of `ports/core`, and `make build KDOS_BOARD=<board>`
producing a boot image. Every task below builds on that tree and does not modify it.

Three exclusions from that plan are **reversed here** and one is not:

| `mobile.plan.md` says | This plan |
|---|---|
| no display, no touch, no desktop | **in** — the whole point |
| no appbox / podman | **in** — Wave 8, curated arm64 image |
| no Wi-Fi | **in** — Wave 7, `qrtr` + `pd-mapper` + `rmtfs` + ath10k |
| no modem | **still out** — named below, a later arc |

---

## Global Constraints

### Provenance rule

Every pixel count, millimetre and dpi figure below is **derived from two measured numbers** — the
panel's resolution and its diagonal — and the derivation is written beside it. Every kernel,
firmware and Debian fact is **verified against upstream at the moment it is used, never
recalled**. Changing a derived number means redoing its arithmetic; changing an upstream fact
means re-reading its source.

### Derived geometry — every number and how it was got

The OnePlus 6T panel is **1080 × 2340** at **6.41 inches**. Everything else follows:

```
diagonal px  = sqrt(1080² + 2340²) = sqrt(6 642 000)  = 2577.2 px
density      = 2577.2 / 6.41                          = 402.06 dpi
px per mm    = 402.06 / 25.4                          = 15.829 px/mm
```

**Nothing hardcodes 402.** The density is computed at run time from `wl_output.geometry`'s
`physical_width` / `physical_height`, which libkwl does not currently record and must start to.
An output reporting **0 mm** — which many do, and every headless backend does — falls back to the
fixed defaults in the last column.

| Quantity | Rule | On fajita | Fallback |
|---|---|---|---|
| chrome cell, default | `32 × 64` px | **33 × 36** portrait, **73 × 16** landscape | same |
| cell ladder (pinch rungs) | `24×48 · 32×64 · 48×96 · 64×128` | `45×48 · 33×36 · 22×24 · 16×18` portrait | same |
| general touch target | ≥ **7 mm** = 111 px → `ceil(111 / cell_h)` rows | 2 rows @ cell 64 = 128 px = 8.09 mm | 2 rows |
| general touch target, width | `ceil(111 / cell_w)` cols | 4 cols @ cell 32 = 128 px = 8.09 mm | 4 cols |
| keyboard key, width | ≥ **6 mm** = 95 px | 10 keys across 1080 = 108 px = 6.82 mm | — |
| keyboard key, height, portrait | ≥ **8 mm** = 127 px | 2 rows @ cell 64 = 128 px = 8.09 mm | — |
| keyboard key, height, landscape | ≥ **7 mm** = 111 px | 4 rows, see below | — |
| touch slop (tap vs drag) | **2 mm** = 32 px | 32 px | 16 px |
| long-press | **500 ms** | | |
| double-tap window | **300 ms** | | |
| gesture edge band | **5 mm** = 79 px | 79 px | 40 px |
| gesture commit distance | **15 mm** = 237 px | 237 px | 120 px |
| kinetic velocity window | last **100 ms** of motion | | |
| kinetic decay | to zero over **1.2 s** | | |
| pinch rung commit | **±20 %** from the reference distance | | |

**A key is not a button, and the floors differ deliberately.** A general control is hit once,
from anywhere, with no expectation about where the next one is; 7 mm is the floor for that. A
keyboard key sits in a dense predictable grid the hand learns, which is why every phone keyboard
ever shipped has keys narrower than its buttons. 6 mm × 8 mm is the floor here, and 10 keys
across 1080 px lands at 6.82 mm — inside it, but only just, which is why the number is written
down rather than assumed.

**In landscape the portrait floor cannot be satisfied and the plan says so rather than
pretending.** A landscape screen is 1080 px tall; five key rows at the 8 mm floor is
5 × 127 = 635 px, which is 59 % of the screen. So **landscape uses four key rows and a 7 mm
height floor** — 4 × 111 = 444 px = 41 % — and the number row is reached by the flick-up layer
instead of occupying a row of its own. That is what makes swipe-on-key load-bearing rather than
decorative.

| | rows | height | share of screen |
|---|---|---|---|
| portrait, 5 key rows + 2-row strip | 5 × 128 + 128 = 768 px | of 2340 | **32.8 %** ≤ 45 % ✓ |
| landscape, 4 key rows + 1-row strip | 4 × 111 + 64 = 508 px | of 1080 | **47.0 %** ≤ 50 % ✓ |

**`kdos-osk` computes this at startup and REFUSES to draw with a log line naming the numbers if
it cannot satisfy both the key floor and the screen share.** A keyboard that silently draws keys
nobody can hit is worse than one that says why it will not.

### Decisions taken

Recorded so nobody re-opens them, and so a reader knows which alternatives were considered and
rejected rather than never thought of.

| # | Decision | Rejected |
|---|---|---|
| 1 | **Navigation:** N9 Harmattan edge-swipe + webOS card stack + a Symbian/BlackBerry soft-key row | Android 3-button nav bar; PalmOS fixed silkscreen row |
| 2 | **OSK protocol:** both, `osk_mode = keys \| im`, `keys` default | virtual-keyboard only; input-method only |
| 3 | **Packaging:** two new packages, `kdos-mobile` + `kdos-osk` | one package; extending `kdos-shell`'s `TOOLS[]` |
| 4 | **Convergence:** full scope in this plan, DP alt-mode included | design-only; out of scope |
| 5 | **Home:** four panes — Events / Apps / Running / **Console** | N9's three; two panes with a shade |
| 6 | **Zoom:** pinch steps the CELL SIZE and re-lays out, chrome **and** apps | chrome only; a settings-only text size |
| 7 | **Prediction:** a completion strip that **never rewrites** what was typed | none at all; Android/iOS autocorrect |
| 8 | **Touch proof:** QEMU multi-touch injection as the acceptance gate | recorded trace replay in `selftest.sh` |
| 9 | **OSK extras:** spacebar-as-trackpad; flick-up/flick-down key layers | per-app layout switching; split landscape keyboard |
| 10 | **Workstation:** cell-granular selection; landscape split-screen; gestures bound to commands | one-handed reachability |
| 11 | **Appbox:** every app in the arm64 image reachable from the phone screen | docked-mode only; no appbox at all |
| 12 | **arm64 image:** curated, ~25 phone-sensible apps, committed in-tree | the full ~105 set; host-fetch-only |
| 13 | **CRT pass:** scanline period scaled to the display, backed off on battery | full desktop pass always; off on mobile |
| 14 | **Oversized windows:** scale by default, pinch to zoom, drag to pan | pan only; report the real size and cope |
| 15 | **Radio:** Wi-Fi in, telephony out | everything out; everything in |
| 16 | **Wave order:** the screen first | keyboard first; touch plumbing first |

**Decisions 11 and 12 interact and are resolved as follows:** the arm64 image is **curated at
bake time** to roughly 25 applications that make sense on a phone, and **every application in
that image is reachable from the mobile launcher on the phone screen** — there is no
docked-only gating. Curation happens once, in `Containerfile.mobile`; reachability is
unconditional.

### Hard rules

1. **`kdos-shell` is not modified.** The desktop taskbar and its 28 names keep working
   byte-identically. The only code that moves out of it is the shared application index and the
   favourites writer, which move **into libraries** (see Task 5.1) precisely so that two shells
   cannot grow two answers to "what is an app".
2. **Every touch path must degrade to the mouse path.** A surface that only works with real touch
   is a surface that is broken under `--dump`, on a tty, under emulated pointer and on a
   touchscreen laptop with a mouse plugged in.
3. **`kicon_slot()` may answer -1 and every consumer must still draw.** The existing rule, and it
   is what keeps the goldens honest.
4. **No hover-only affordance, anywhere in the mobile chrome.** `wl_touch` has no hover. Anything
   a pointer learns by hovering must be reachable by long-press.
5. **Nothing blocks the frame.** The gesture recogniser, the completion trie, the haptic write
   and the thumbnail render all run off the frame path or not at all — the rule the tray, the
   frames socket and the privacy indicator already keep.
6. **No `system()` and no shell string anywhere in either new package.** Gestures may name a
   command; it is split with `kxdg_exec_split()` and exec'd through `KbArgv`, exactly as every
   other launch path in this tree does.
7. **Every source file carries the KDOS ASCII banner**, comment-prefixed for its language.
8. **No `kpkgbuild` rationale comments** (KDOS hard rule 4). Reasoning goes in this file or a
   commit message.
9. **No source edits with `sed`/`awk`** (KDOS hard rule 7). Build flags, or a real `.patch`
   beside the recipe.
10. **Do not auto-commit** (KDOS hard rule 5); no destructive `git` (hard rule 6).
11. **Config files are PARSED, never sourced**, and an unrecognised key is **reported by name**,
    never silently ignored — the promise `comp.conf` already makes.
12. **Builds stay offline** (`--network none`). Only `make fetch`, the firmware fetch and
    `make fetch-apps` touch the network.

### Facts that are easy to get wrong

- **The press must be DEFERRED until tap-vs-drag is decided.** Emitting a press on touch-down and
  a drag afterwards means that starting a scroll on a list row selects that row and then
  activates it. This is the single most consequential rule in the whole plan and it is the touch
  analogue of the `pt_motion` cell-dedup lesson already in `CLAUDE.md`.
- **`wl_touch.cancel` is not optional and labwc does not send it.** Verified: there is no
  `cancel` path in `src/desktop/kdos-comp/src/input/touch.c`. Without one, an edge swipe that the
  compositor claims for "go home" *also* activates whatever the finger started on.
- **`kdos-cmd.c` is request/response and its own header says so** — *"[anything that] wants a
  stream of events wants kdos-frames.sock, which is that."* The OSK's "a text field took focus"
  signal is a STREAM and must not be bolted onto the command socket.
- **`zwlr_layer_shell_v1` must be bound at 4.** Already a rule in this tree; it matters twice
  here, because the OSK and the shade both want `KEYBOARD_INTERACTIVITY_ON_DEMAND` and a client
  bound at 1 is granted EXCLUSIVE instead.
- **A layer surface with `exclusive_zone = 0` is placed inside the usable area, which already has
  the panels' zones taken out of it.** Applying a margin on top of that is the popup-floating-one
  bar-height-too-high bug already recorded in `CLAUDE.md`. Zone **-1** is the answer wherever a
  margin is the offset.
- **A repeated fontconfig property APPENDS.** `Terminus:pixelsize=32:pixelsize=64` resolves to
  the FIRST. The size a font name carries must be stripped before a new one is appended — the
  trap `libkchrome`'s tile code already documents, and pinch-to-zoom hits it on every rung.
- **A bitmap font cannot be asked for an arbitrary size.** Terminus is a PCF; a request for 96 is
  answered with the nearest strike. The result must be MEASURED and a much-shorter answer retried
  against `:scalable=true`. `terminus-ttf` is the scalable face this tree already ships for
  exactly this.
- **The console font is 512 glyphs.** Mobile chrome that must also read on a tty stays inside the
  vt tier — `░ ▒ █`, the box-drawing sets, `◀ ▶`. No half blocks, no braille, no `← →`.
- **Debian trixie is not uniformly arm64.** Every package in `Containerfile.mobile` is verified
  to exist for arm64 at bake time; an assumed one fails the bake, not the plan.
- **`mkfs`/`podman` traps from the desktop appbox apply verbatim** to the arm64 image: wipe
  `$STORAGE` before loading, drop root's runroot from the libpod database, flatten the loaded
  image to one layer, keep the uid remap idempotent. Each already cost a debug cycle once.

### Verified findings this plan rests on

Each was **measured in this repo**, not assumed. If one turns out false during execution, stop
and re-plan rather than working around it.

1. **libkwl binds no touch at all.** `seat_caps` in `src/libs/libkwl/kwl.c` handles KEYBOARD and
   POINTER only, and `kwl.h` contains no touch symbol. Every touch today reaches KDOS chrome as
   an emulated pointer event.
2. **The labwc fork already relays `wl_touch` and emulates a pointer for clients that do not bind
   it** — `src/input/touch.c` calls `wlr_seat_touch_notify_down/motion/up` and
   `cursor_emulate_move_absolute` / `cursor_emulate_button`. Tap-as-click therefore works before
   any of this plan lands, which is what lets Waves 4 and 5 be developed against the desktop.
3. **`src/input/touch.c` has no `cancel` handling.** Grepped for `cancel`,
   `wlr_seat_touch_notify_cancel`, `touch_cancel`: no hits.
4. **`src/input/gestures.c` is `wlr_pointer_gestures_v1` — TOUCHPAD pinch and swipe, not
   touchscreen.** Touchscreen multi-finger gestures do not exist in the fork.
5. **`config/touch.c` exists** and already carries per-device touch configuration including
   `force_mouse_emulation` and output mapping.
6. **`wlr_scene_buffer_set_dest_size()` is used in five places in the fork**, including
   `kdos-wallpaper.c`, `workspaces.c` and `cycle/osd-thumbnail.c`. Scaling a scene buffer is
   established practice here, which is what makes `kdos-fit.c` a graft rather than a research
   project.
7. **`cycle/osd-thumbnail.c` already renders a view's scene tree into a buffer** (`render_node`,
   `render_thumb`). The card switcher's thumbnails have a working implementation to reuse.
8. **`kdos-child.c` spawns chrome PER OUTPUT** from labwc's output-created hook, with `per_output`
   on each `TEMPLATES[]` row and a `stopping` state on output destroy. Convergence needs a second
   template SET, not a new mechanism.
9. **`kdos-cmd.c` is request/response only.** Its header comment names `kdos-frames.sock` as the
   thing to use for a stream.
10. **`libkcell` already renders an image as cells** — `kcell_ascii_image`, used by `kdos-devices`
    for its camera preview. ASCII window thumbnails need no new renderer.
11. **`kdos-shell/main.c`'s `TOOLS[]` has 28 entries** and dispatch is on `argv[0]`'s basename
    with an `argv[1]` fallback. `kdos-mobile` copies that shape exactly.
12. **`KtuiEvent` carries `mx, my` in CELLS and nothing in pixels** (`src/libs/libktui/ktui.h`).
    Kinetic scroll, drag thresholds and pinch all need sub-cell precision, so the struct must
    grow.
13. **`testing/qemu-hw/Dockerfile` is `ubuntu:25.10` and installs `qemu-system-x86` only.**
    QEMU 10 has `virtio-multitouch-pci` and QMP multi-touch `input-send-event`;
    `qemu-system-arm` has to be added to the image for an aarch64 rig.
14. **No on-screen keyboard port exists** in `ports/core` (430 ports listed; no squeekboard, no
    wvkbd, no onboard). `libinput`, `libevdev` and `mtdev` are present.
15. **`fcitx5` is ported and binds `zwp_input_method_manager_v2`**, and labwc permits one input
    method per seat. This is why decision 2 exists.

### What is explicitly NOT in this arc

Named so nobody adds them opportunistically. Each is a later arc.

- **No telephony.** No `ModemManager`, no dialer, no SMS, no call audio routing, no VoLTE, no SIM
  PIN, no emergency calling. The SDM845 modem path on mainline is the least reliable part of the
  stack and would gate the shell work — which is what this plan is *for* — behind it.
- **No GPS, no camera capture pipeline, no fingerprint reader, no NFC.**
- **No one-handed / reachability mode.** Considered and not chosen.
- **No per-app keyboard layout switching.** The completion strip is focus-aware (it becomes the
  hacker row when a terminal is focused); the LAYOUT itself does not change per application.
- **No split landscape keyboard.** Considered and not chosen.
- **No autocorrect.** The completion strip offers and never rewrites, and that is a policy, not a
  first version.
- **No `com.canonical.dbusmenu` rendering.** Still the tray's stated gap, on mobile as on
  desktop.
- **No drag-and-drop between applications.** `start_drag` is still a comment in `kwl.c`; touch
  makes it more desirable and no less absent.
- **No fractional scaling.** `wp_fractional_scale_v1` is still unbound. The cell ladder is the
  integer answer and is the better one here.
- **No A/B slots, no encrypted root, no `kinstall`** on mobile — `mobile.plan.md`'s exclusions
  stand.

---

## The innovation register

What in this plan exists nowhere else, and why KDOS specifically can have it. This section is
the answer to *"why not just run Plasma Mobile"*, and each row names the task that builds it.

| # | Thing | Why only here | Task |
|---|---|---|---|
| I1 | **Pinch changes the CELL SIZE and re-lays out** — every zoom level is real glyphs at a real size, never a resampled bitmap | The whole desktop is a character grid; a zoom is a resize, and the resize path already exists | 2.3, 3.2 |
| I2 | **Cell-granular selection over ANY surface** — long-press, drag a rectangle, copy, with no application support at all | Everything on screen is cells the compositor and the shell both understand | 5.7 |
| I3 | **Convergence for free** — mobile chrome on the touchscreen and the full desktop taskbar on an external monitor, simultaneously | `kdos-child.c` already spawns chrome per output; it needs a second template set, not a new architecture | 6.1, 6.2 |
| I4 | **A gesture can run a shell command** — a programmable window manager on a phone | The compositor already has a command socket and `kdos hey` already speaks to it | 3.2 |
| I5 | **The task switcher shows each window as ASCII art of itself** | `kcell_ascii_image` already exists for the camera preview | 5.4 |
| I6 | **A Console pane on the home screen** — a tmux session you swipe to and never lose | A Linux phone is the only phone where this is the point rather than a novelty | 5.3 |
| I7 | **A keyboard with Ctrl, Alt, Esc, Fn and arrows that latch visibly** — Hacker's Keyboard as a first-class citizen, not an add-on | The entire distro is keyboard-driven: foot, mc, neovim, btop, tmux | 4.3 |
| I8 | **Slide the spacebar to move the caret; latched-Shift-slide to select** | Text editing on a touchscreen is the unsolved problem on every Linux phone | 4.5 |
| I9 | **The CRT identity survives at 402 dpi** — the scanline period scales with the display so the phone, the TTY and the boot splash still match | The pass is one shader the whole system renders through, not four imitations | 6.5 |
| I10 | **The oversized-window answer is honest** — a desktop app is configured at a virtual size and scaled to fit, so no OK button is ever unreachable | `wlr_scene_buffer_set_dest_size` is already used five times in this tree | 6.3 |

---

## Target Tree

Everything marked **NEW** is created by this plan. Everything marked **EDIT** is an existing file
this plan changes; `kdos-shell/` appears nowhere in either list except where code moves OUT of it
into a library.

```
src/
├── libs/
│   ├── libkwl/
│   │   ├── kwl.h                       EDIT  touch API, output mm, cell size setter
│   │   ├── kwl.c                       EDIT  seat_caps binds wl_touch; wl_output geometry
│   │   ├── kwl_priv.h                  EDIT  touch point table, gesture state
│   │   └── kwl_touch.c                 NEW   down/up/motion/frame/cancel, tap-vs-drag,
│   │                                         long-press, kinetic, pinch, 2-finger pan
│   ├── libktui/
│   │   └── ktui.h                       EDIT  KtuiEvent gains px/py/tid/gest/gval;
│   │                                         KT_EVT_TOUCH, KT_EVT_GESTURE, KT_G_*;
│   │                                         ktui_touch_min_rows/cols
│   ├── libkchrome/
│   │   ├── kchrome.h                    EDIT
│   │   ├── kch_kinetic.c                NEW   kinetic scroll state for kch_list_*
│   │   ├── kch_sheet.c                  NEW   the bottom sheet (mobile's modal)
│   │   └── kch_fav.c                    NEW   favourites reader/writer, moved out of
│   │                                         kdos-shell/fav.c
│   └── libkxdg/
│       ├── kxdg.h                       EDIT
│       └── kxdg_apps.c                  NEW   the ONE application index, moved out of
│                                              kdos-shell/apps.c
├── desktop/
│   ├── kdos-comp/
│   │   ├── include/kdos.h               EDIT  graft declarations, new conf fields
│   │   └── src/
│   │       ├── kdos-config.c            EDIT  the mobile keys
│   │       ├── kdos-child.c             EDIT  per-output TEMPLATE SETS
│   │       ├── kdos-crt.c               EDIT  scanline period × scale; battery floor
│   │       ├── kdos-gesture.c           NEW   touchscreen gesture recogniser + cancel
│   │       ├── kdos-input.c             NEW   $XDG_RUNTIME_DIR/kdos-input.sock, the
│   │       │                                  text-field-focus / mode event STREAM
│   │       ├── kdos-mode.c              NEW   form-factor classification per output/seat
│   │       ├── kdos-fit.c               NEW   oversized-window scale / pan
│   │       ├── kdos-rotate.c            NEW   IIO accelerometer -> output transform
│   │       ├── input/touch.c            EDIT  cancel path; hand-off to kdos-gesture
│   │       ├── main.c                   EDIT  init/finish hooks for the five new grafts
│   │       └── meson.build              EDIT
│   ├── kdos-lock/                       EDIT  --pin: a PIN pad and lock-screen notifications
│   ├── kdos-osk/                        NEW   ── the on-screen keyboard package ──
│   │   ├── kpkgbuild
│   │   ├── build.sh
│   │   ├── osk.h
│   │   ├── main.c                       CLI, basename dispatch, the surface + poll loop
│   │   ├── layout.c                     the .layout parser and the layout table
│   │   ├── render.c                     cell-grid drawing, latch state, key preview
│   │   ├── keys.c                       virtual-keyboard-v1: keymap upload, keycodes,
│   │   │                                modifier latching
│   │   ├── im.c                         input-method-v2 path (osk_mode = im)
│   │   ├── predict.c                    the completion trie + typed history
│   │   ├── gesture.c                    spacebar trackpad, flick-up/down key layers
│   │   ├── haptic.c                     the vibrator write, fire-and-forget
│   │   ├── dump.c                       --dump and --dump-layout [--mm]
│   │   └── layouts/                     qwerty.layout hacker.layout numeric.layout
│   │                                    symbols.layout  -> /usr/share/kdos/osk/
│   └── kdos-mobile/                     NEW   ── the touchscreen shell package ──
│       ├── kpkgbuild
│       ├── build.sh
│       ├── mobile.h
│       ├── main.c                       basename dispatch, TOOLS[]
│       ├── bar.c                        kdos-bar     — the status bar
│       ├── nav.c                        kdos-nav     — soft keys + gesture affordance
│       ├── home.c                       kdos-home    — the four-pane home + search
│       ├── panes.c                      the Events / Apps / Running / Console panes
│       ├── cards.c                      kdos-cards   — the webOS card switcher
│       ├── shade.c                      kdos-shade   — notifications + quick settings
│       ├── sheet.c                      kdos-sheet   — the bottom-sheet dialog front end
│       └── sel.c                        kdos-sel     — cell-granular selection overlay
├── packages/
│   └── kdos-tools/                      EDIT  `kdos hey` gains the mobile queries;
│                                              `kdos help` gains the gesture cheat sheet
ports/
├── mobile/
│   ├── mesa/                            NEW   freedreno gallium + vulkan, aarch64
│   ├── qrtr/                            NEW   Wave 7
│   ├── pd-mapper/                       NEW   Wave 7
│   └── rmtfs/                           NEW   Wave 7
└── appbox/
    ├── Containerfile.mobile             NEW   arm64, ~25 curated apps
    └── image-arm64/                     NEW   LFS chunks + INDEX.json  (~1.2 GB)
script-mobile/
├── 06_mobile_desktop/                   NEW   packages.txt for the mobile desktop set
├── mobile_desktop.env.sh                NEW
└── 07_packaging/                        EDIT  install the OSK layouts, the skel config
fs/
├── etc/skel/.config/kdos/
│   ├── gestures.conf                    NEW   shipped commented, defaults documented
│   └── osk.conf                         NEW   shipped commented
└── etc/udev/rules.d/70-kdos-haptic.rules NEW  the vibrator, to the `input` group
testing/
├── qemu-mobile/                         NEW   aarch64 rig: virtio-gpu-gl, multitouch, QMP
│   ├── Dockerfile
│   ├── run.sh
│   └── touch.py                         QMP multi-touch injection + trace replay
├── touch/*.trace                        NEW   recorded gesture traces
├── goldens/mobile-*                     NEW   33x36 and 73x16 for every mobile surface
├── preflight.sh                         EDIT  gestures.conf, layouts, flag agreement,
│                                              key-size audit
└── selftest.sh                          EDIT  the new library assertions and goldens
Makefile                                 EDIT  run-mobile, test-mobile, fetch-apps ARCH
CLAUDE.md                                EDIT  the mobile sections
```

---

## Wave 1 — The screen

**Gate: a KDOS compositor draws on a display on both boards.** Nothing else in this plan can be
judged until something is on a screen. The wave splits deliberately: Tasks 1.1–1.3 need no phone
and are done under QEMU; Task 1.4 is **MANUAL** and needs the device, exactly as
`mobile.plan.md`'s Task 12 does.

### Task 1.1: The aarch64 display stack

- [ ] Add `ports/mobile/mesa/` overriding `ports/core/mesa`: `-Dgallium-drivers=freedreno`,
      `-Dvulkan-drivers=freedreno`, `-Dplatforms=wayland`, `-Dglx=disabled`. Keep every other
      flag identical to the core recipe so the two do not drift for reasons nobody recorded.
- [ ] Verify `wlroots`, `libdrm`, `libinput`, `libxkbcommon`, `pixman`, `fcft`, `libdisplay-info`
      and `seatd` build unmodified for aarch64 under the qemu-user chroot. Any that do not get a
      `ports/mobile/` override; **record which and why in this task's checkpoint**, because a
      silent override is how a mobile tree diverges.
- [ ] Verify `kdos-comp`, `kdos-shell`, `kdos-lock`, `kdos-powerd`, `kdos-notifyd` and the whole
      `src/libs/` set compile for aarch64 with no source change. They are arch-clean today; this
      is the assertion, not the assumption.
- [ ] Add `script-mobile/06_mobile_desktop/packages.txt` and `mobile_desktop.env.sh` (parsed
      metadata block, `KDOS_SNAPSHOT_PATHS`, banner header, the five reproducibility lines).
      Ordering: it sorts after the base userland phases and before packaging, mirroring how
      `05_desktop` sorts before `05_phase5` on the desktop tree.

**Checkpoint**
```sh
make build KDOS_BOARD=qemu-aarch64 BUILD_ARGS="--phases 06_mobile_desktop"
ls build-mobile/fs/usr/bin/kdos-comp
ls build-mobile/fs/usr/lib/dri/ | grep -i freedreno
```
Expected: `kdos-comp` present; a freedreno DRI driver present; the phase log records every
`ports/mobile/` override with its reason.

### Task 1.2: A compositor session on the mobile tree

- [ ] Port `fs/usr/local/bin/kdos-desktop` and `kdos-desktop-start` to the mobile rootfs. They
      stay `/bin/sh` (KDOS rule: every line in them is a fix for something that broke). The
      mobile copies differ in exactly two ways and both are recorded in the file: no appbox
      warmup until Wave 8, and `XDG_CURRENT_DESKTOP=KDOS`.
- [ ] Ship a mobile `~/.config/kdos-comp/rc.xml` in skel. **`<default />` MUST be the first child
      of `<keyboard>` and of `<mouse>`** — the single most load-bearing line in this distro's
      configuration, and `testing/preflight.sh` already fails a skel `rc.xml` that gets it wrong.
- [ ] Ship a mobile `~/.config/kdos/comp.conf` in skel with the mobile defaults from Task 6.5
      commented in place, so a reader can find the knobs without reading this plan.
- [ ] `~/.config/kdos-comp/menu.xml`: the root menu is reachable on a phone only through
      `kdos-nav`'s Menu soft key, so it stays what it is and gains nothing.

**Checkpoint**
```sh
testing/preflight.sh 2>&1 | grep -E 'rc.xml|default'
```
Expected: the `<default />` check passes for the mobile skel `rc.xml`.

### Task 1.3: QEMU virt — the compositor draws

- [ ] Create `testing/qemu-mobile/Dockerfile` from `testing/qemu-hw/Dockerfile`, adding
      `qemu-system-arm` to the package list. **Verify in the built image** that
      `qemu-system-aarch64 -device help` lists `virtio-gpu-gl-pci` and `virtio-multitouch-pci`;
      a rig built on a QEMU without them must fail here, not in Wave 2.
- [ ] `testing/qemu-mobile/run.sh`: `-M virt -cpu cortex-a72`, `-device virtio-gpu-gl-pci`,
      `-display egl-headless`, `-vnc :N`, `-device virtio-multitouch-pci`,
      `-device virtio-keyboard-pci`, a QMP socket and a serial socket. Resolution is set to
      **1080x2340** so every golden and every millimetre figure in this plan is exercised at the
      real aspect ratio.
- [ ] Drive the session the way `testing/vnc-shot.py` already does: type `kdos-desktop` on the
      console through the monitor, wait for `kdos-comp`, read the framebuffer over RFB.
      `SetEncodings` is `type(1) pad(1) count(2)` — an extra padding field desyncs the stream and
      every later read blocks.
- [ ] **Report the renderer.** `egl-headless` + `virtio-gpu-gl` should give GLES2 and therefore
      the CRT pass; plain `virtio-gpu` gives pixman and no pass. The rig prints which it got, so
      a screenshot is never misread as "the shader is broken".

**Checkpoint**
```sh
make run-mobile-shot BOARD=qemu-aarch64 OUT=/tmp/w1.ppm
head -2 /tmp/w1.ppm            # P6 / 1080 2340
```
Expected: a 1080x2340 framebuffer containing the wallpaper, and a log line naming the renderer.

### Task 1.4: fajita — panel, backlight, touchscreen — **MANUAL, needs the device**

- [ ] Verify against the sdm845-mainline tree and pmaports `device-oneplus-fajita` — **read them,
      do not recall** — which of `CONFIG_DRM_MSM`, the DSI panel driver, the backlight driver and
      the touchscreen driver the KDOS kernel fragment must carry, and add them.
- [ ] Add the touchscreen and any `gpio-keys` nodes to the udev rules so the seat user can open
      them, and verify `libinput list-devices` sees a touch device with the right absolute axes
      and physical dimensions. **A touchscreen reporting 0 mm makes every millimetre in this
      plan fall back**, so the reported size is checked here and recorded.
- [ ] Bring up `/sys/class/backlight` and confirm a write changes the panel.
- [ ] Boot `kdos-comp` on the device and photograph the wallpaper.
- [ ] Record the ACTUAL reported dpi from `wl_output.geometry` beside the 402.06 derived above.
      If they disagree, the derived table is annotated, not silently replaced.

**Checkpoint** — pasted into this task by hand:
```
kdos-comp on fajita:  renderer=<gles2|pixman>  output=<name> <W>x<H> <mm>x<mm>
libinput touch device: <name>  abs axes <x0..x1> <y0..y1>
backlight: <path>  min=<n> max=<n>  write verified
```

---

## Wave 2 — Touch in libkwl

**Gate: a finger drives every existing kdos-shell surface correctly, with no change to any of
them.** That is the acceptance test for this wave and it is deliberately about the OLD code:
if `kdos-pick` and `kdos-settings` scroll, select and open a context menu correctly under a
finger, the touch layer is right and everything built after it inherits that.

### Task 2.1: Bind `wl_touch`, and record the output's physical size

- [ ] `kwl.c`: `seat_caps` gains `WL_SEAT_CAPABILITY_TOUCH`, creating and destroying the
      `wl_touch` alongside the pointer and keyboard.
- [ ] `kwl.c`: the `wl_output` listener records `physical_width` / `physical_height` from
      `geometry`, and `kwl.h` exposes `kwl_output_mm(int *w, int *h)` and
      `kwl_px_per_mm(void)`. **Zero is a legitimate answer** and every caller must treat it as
      "unknown", falling back to the constants in the geometry table, never to a division by
      zero.
- [ ] `kwl_priv.h`: a fixed table of touch points (10 slots is what every touchscreen this tree
      will meet reports; an 11th is dropped with a debug line rather than growing an allocation
      on the input path).
- [ ] `kwl_touch.c` NEW: `down` / `up` / `motion` / `frame` / `cancel` handlers. **`frame` is the
      commit point** — nothing is emitted from the individual handlers, exactly as `pt_frame` is
      the commit point for the wheel. **`down` seeds the position**, the `pt_enter` lesson.

**Checkpoint**
```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh 2>&1 | tail -20
```
Expected: clean. libkwl compiles with the touch listener installed; `kwl_output_mm` returns the
compositor's numbers under the QEMU rig and `0, 0` under a headless backend.

### Task 2.2: Tap, drag and long-press — the deferred press

**This is the most important task in the plan.** Get it wrong and every list on the device
activates the row you started scrolling from.

- [ ] A touch-down emits `KT_EVT_TOUCH` (down) immediately — that is what a button uses to draw
      itself pressed — and emits **no mouse event at all**.
- [ ] The interaction resolves into exactly one of four outcomes and each emits a different
      thing:

  | Outcome | Condition | Emits |
  |---|---|---|
  | **tap** | released within 500 ms having moved < slop | `KT_MB_LEFT` press **and** release, at the DOWN cell |
  | **long-press** | held 500 ms having moved < slop | `KT_MB_RIGHT` press + release, at the down cell, plus a haptic pulse |
  | **drag** | moved ≥ slop (2 mm) before release | `KT_MP_DRAG` motions from the crossing point on; **never a press** |
  | **cancelled** | `wl_touch.cancel`, or the point vanishes | `KT_EVT_TOUCH` (cancel) and nothing else — the interaction is ABANDONED, not completed |

- [ ] The press for a tap is emitted at the **down** cell, not the up cell. A finger rolls; the
      cell it lifts from is not the one it aimed at.
- [ ] `KT_MB_MOVE` is **never** synthesised from touch. There is no hover, and manufacturing one
      would light hover affordances under a finger that is about to leave.
- [ ] Touch motion is **not** cell-deduped — a drag needs every pixel. The mouse `KT_MP_DRAG`
      events synthesised from it **are** cell-deduped, because that is what every existing
      consumer expects and what the `pt_motion` rule already establishes.
- [ ] Double-tap is recognised (300 ms) and reported as `KT_EVT_GESTURE` / `KT_G_DOUBLETAP`.
      Nothing in this plan consumes it; it exists because a second tap otherwise reads as two
      unrelated taps and a consumer that wants the distinction cannot recover it.

**Checkpoint** — under the QEMU rig, with the shipped `kdos-pick` unmodified:
```sh
testing/qemu-mobile/touch.py tap    540 1200   # a row activates
testing/qemu-mobile/touch.py drag   540 1200 540 400   # the list SCROLLS, nothing activates
testing/qemu-mobile/touch.py hold   540 1200   # the context menu opens
```
Expected: three different outcomes, and in particular the drag activates nothing.

### Task 2.3: Kinetic scroll, pinch and two-finger pan (I1)

- [ ] `kwl_touch.c` keeps a velocity estimate over the last **100 ms** of motion. At release, if
      the interaction was a drag and the velocity is non-trivial, it emits synthetic wheel ticks
      on a decaying schedule that reaches zero in **1.2 s**.
- [ ] The kinetic scroll must shorten the caller's poll deadline while it runs — the pattern the
      meters strip, the launch pulse and libkwl's own key repeat already use. `kwl_next_deadline_ms()`
      is the shared accessor; a caller that ignores it gets a scroll that advances at whatever
      cadence it happened to poll at, which is the exact bug key repeat was added to fix.
- [ ] **A touch-down during a kinetic scroll stops it and is consumed** — it does not also tap.
      Every touch platform does this and its absence is instantly noticeable.
- [ ] Pinch: with exactly two points down, `KT_EVT_GESTURE` / `KT_G_PINCH` carries
      `gval = current_distance / reference_distance`. A rung commits at **±20 %**, and the
      reference distance is **reset to the current distance on commit**, so a continuous pinch
      steps one rung at a time instead of running the whole ladder in one motion.
- [ ] `kwl_cell_size_set(px)` steps the chrome font's pixel size along the ladder. **Strip the
      size the font name already carries before appending a new one** — a repeated fontconfig
      property appends, and `Terminus:pixelsize=32:pixelsize=64` resolves to 32. **Measure the
      result**: a bitmap face answers a request for 96 with its nearest strike, so a face that
      comes back much shorter than asked is retried against `:scalable=true` and the closer of
      the two wins. `terminus-ttf` is what this tree ships for exactly this.
- [ ] Changing the cell size sets `ktui_resized`. **Zoom therefore reuses the resize path
      entirely** — no consumer needs a zoom-aware code path, and any consumer that already
      handles resize correctly handles zoom correctly. Any that does not (`kdos-notifyd` was the
      one that did not) is fixed here.
- [ ] Two-finger pan: `KT_G_PAN2` with a pixel delta. Consumed by `kdos-fit` in Wave 6 and by
      nothing before it.

**Checkpoint**
```sh
testing/qemu-mobile/touch.py fling  540 1800 540 400   # scrolls on past the release
testing/qemu-mobile/touch.py pinch-out                  # one rung: 33x36 -> 22x24
testing/qemu-mobile/touch.py pinch-out                  # one more rung: 22x24 -> 16x18
```
Expected: the fling continues and decays; each pinch steps exactly one rung; the text is sharp
at every rung, which is visible in the VNC screenshot as a real glyph change, not a blur.

### Task 2.4: The touch-target floor in libktui and libkchrome

- [ ] `ktui.h`: `ktui_touch_min_rows()` and `ktui_touch_min_cols()` return
      `ceil(111 / cell_h)` and `ceil(111 / cell_w)` from the live px-per-mm, and **2 and 4**
      when the output reports no physical size.
- [ ] `kch_list_*` gains a mode where each row occupies `ktui_touch_min_rows()` rows and the
      selection, the hit test and the scrollbar all use it. The existing one-row desktop
      behaviour is untouched and is still the default; mobile front ends opt in.
- [ ] `kch_sheet.c` NEW: the bottom sheet. Anchored to the bottom edge with
      **`exclusive_zone = -1`** — the protocol's "do not move me out of anyone's exclusive zone",
      which is what makes a caller-supplied margin the only offset there is. A sheet is dragged
      down to dismiss; the drag threshold is the same 2 mm slop.
- [ ] `kch_kinetic.c` NEW: the per-list kinetic state, so every list on the system scrolls with
      the same feel and there is one implementation of that feel.

**Checkpoint**
```sh
kdos-pick --dump --dump-size 33x36 | head -40
```
Expected: rows two cells tall, no row overlapping the frame, the scrollbar on the frame's own
right edge with the rows giving up a cell rather than the bar overdrawing them.

### Task 2.5: The QEMU multi-touch rig

- [ ] `testing/qemu-mobile/touch.py`: QMP `input-send-event` with `InputMultiTouchEvent` —
      slot, tracking-id, ABS_MT_POSITION_X/Y — plus the primitives `tap`, `hold`, `drag`,
      `fling`, `pinch-in`, `pinch-out`, `swipe`, `edge`, and `replay <file>`.
- [ ] `testing/touch/*.trace`: a recorded trace is timestamped `t x y slot state` lines. The
      format is plain text and PARSED, so a trace can be hand-written when a device is not
      available to record one.
- [ ] `make test-mobile` builds the rig image, boots the current `build-mobile` image, replays
      every trace and diffs the resulting framebuffer against a stored PPM.
- [ ] **State the cost in the rig's own output.** A run prints `touch assertions run under QEMU;
      selftest.sh cannot run these` so nobody reads a green `selftest.sh` as covering touch.

**Checkpoint**
```sh
make test-mobile 2>&1 | tail -20
```
Expected: every trace replays, every diff passes, and the caveat line is printed.

---

## Wave 3 — Gestures in the compositor

**Gate: a swipe from the edge goes home, and the app it was swiped over does not also act on
it.** The second half of that sentence is the whole difficulty.

### Task 3.1: `kdos-gesture.c` and the cancel path

- [ ] NEW `src/desktop/kdos-comp/src/kdos-gesture.c`, hooked from `input/touch.c`'s down/motion/
      up handlers before they notify the seat.
- [ ] **The edge band is 5 mm and only a touch that STARTS inside it can start a system
      gesture.** A swipe beginning in the middle of a map is the map's.
- [ ] A gesture is TRACKED from the moment it starts in the band and is **committed** only at
      15 mm of travel in a consistent direction. Until commit, the touch continues to be
      delivered to the client normally.
- [ ] **On commit the client is sent `wl_touch.cancel`** and the touch belongs to the compositor
      for the rest of the interaction. Verified finding: labwc has no cancel path today, so this
      is new code in `input/touch.c`, not a call to something existing.
- [ ] A tracked gesture that is released before commit is **not** replayed — the client already
      received every event, and the compositor never took anything.
- [ ] Multi-finger gestures (`swipe3_*`, `swipe4_*`) are recognised anywhere on the screen, not
      only at an edge, because three fingers are unambiguous in a way one is not.
- [ ] `idle_manager_notify_activity()` is called from the recogniser, the way every other input
      path in the fork does; a gesture must not let the screen dim under the hand.

**Checkpoint**
```sh
testing/qemu-mobile/touch.py edge up      # goes home
testing/qemu-mobile/touch.py edge up-short # 8 mm: nothing happens, the app kept the touch
```
Expected: the first reaches the home surface and the app under it does nothing; the second
leaves the app in the state a short drag would leave it in.

### Task 3.2: `gestures.conf` — and a gesture that runs a command (I4)

- [ ] `~/.config/kdos/gestures.conf`, PARSED never sourced, one `key = value` per line, an
      unknown key **reported by name**. Shipped in skel with every default present and
      commented, the way `comp.conf` is.
- [ ] Builtin actions: `home`, `recents`, `back`, `shade`, `menu`, `zoom in`, `zoom out`,
      `select`, `workspace next`, `workspace prev`, `close`, `none`.
- [ ] **Any other value is a command line**, split with `kxdg_exec_split()` and exec'd through
      `KbArgv` with `child_reset_signals()` — never `system()`, never a shell. This is I4.
- [ ] Shipped defaults:

```
# ~/.config/kdos/gestures.conf — parsed, never sourced.
# An unrecognised key is reported by name; a line that does not take effect says so.
edge_up          = home
edge_up_hold     = recents
edge_left        = back
edge_right       = back
edge_top_pull    = shade
pinch_out        = zoom in
pinch_in         = zoom out
longpress_2      = select
swipe3_up        = none
swipe3_left      = workspace prev
swipe3_right     = workspace next
swipe4_down      = none
# any other value is a command line, split like an Exec= and run without a shell:
# swipe4_down    = kdos-shot
```

- [ ] `zoom in` / `zoom out` are dispatched to the FOCUSED surface, so a pinch on a terminal
      changes foot's font size and a pinch on the shell steps the chrome cell. Both are I1; the
      terminal half is a `SIGWINCH`-shaped answer, not a scaled bitmap.
- [ ] `kdos help` gains a gesture cheat sheet beside the keybind one, and `kdos-keys` gains a
      gestures page. A gesture nobody is told about is a gesture nobody uses — the reason the
      soft-key row exists at all.

**Checkpoint**
```sh
grep -c '=' fs/etc/skel/.config/kdos/gestures.conf
kdos-comp --check-config 2>&1 | grep -i gesture
```
Expected: every shipped key parses; a deliberately mistyped key is reported by name and by line.

### Task 3.3: `kdos-input.sock` — the event STREAM

- [ ] NEW `src/desktop/kdos-comp/src/kdos-input.c`: a listening socket at
      `$XDG_RUNTIME_DIR/kdos-input.sock`, one NDJSON object per event, **non-blocking on both
      ends, no history**. This is `kdos-frames.sock`'s shape exactly, and it is a separate socket
      rather than a message on `kdos-cmd.sock` because that file's own header says a stream
      belongs here.
- [ ] Events:

```json
{"ev":"text_focus","state":"on","purpose":"normal","app_id":"foot"}
{"ev":"text_focus","state":"off"}
{"ev":"mode","output":"DSI-1","form":"handset","touch":true,"keyboard":false}
{"ev":"gesture","name":"edge_up"}
{"ev":"rotate","output":"DSI-1","transform":"90"}
```

- [ ] `text_focus` is emitted from labwc's existing `text-input-v3` relay in `src/input/ime.c` —
      the compositor is already in the middle of that conversation, which is exactly why it, and
      not the keyboard, is the thing that knows a field took focus. `purpose` carries
      `text_input_v3`'s content purpose so the OSK can pick a numeric layout for a number field.
- [ ] **A consumer that cannot keep up loses lines**, and there is no ring. A keyboard that
      connects late has missed what happened, and a buffer would hide that.

**Checkpoint**
```sh
socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/kdos-input.sock &
foot -e sh -c 'read x'      # focus a terminal
```
Expected: a `text_focus` object with `app_id":"foot"` on the stream within one frame.

---

## Wave 4 — `kdos-osk`, the on-screen keyboard

**Gate: `Ctrl-C` kills a process in `foot` on the phone, typed with a thumb.** That single
sentence is the whole reason this keyboard is not a copy of anyone else's: the distro is
keyboard-driven, and a keyboard without Ctrl, Esc, Tab, Fn and arrows makes a Linux phone a
worse Android.

The layout debt is paid to **Hacker's Keyboard**: full five rows, a permanent number row,
F1–F12 behind Fn, arrows, Home/End/PgUp/PgDn/Ins/Del, and modifiers that **latch**.

### Task 4.1: The package and the surface

- [ ] NEW `src/desktop/kdos-osk/` with `kpkgbuild` (banner, `description`, `homepage`,
      `depends`) and `build.sh`. It links **libkwl + libktui + libkcell + libkicon + libkcolor +
      libkbase + libkxdg** and the wayland protocol scanners for `virtual-keyboard-v1` and
      `input-method-v2`. It links **no part of `kdos-mobile`**, because a touchscreen laptop
      running the desktop must be able to install just this.
- [ ] Basename dispatch, one binary: `kdos-osk` and `kdos-oskctl` (the `--toggle` / `--show` /
      `--hide` client, so a gesture or a keybind can reach it without knowing a socket).
- [ ] The surface is `KWL_ROLE_PANEL` on `KWL_EDGE_BOTTOM` **with an exclusive zone**, so a
      window is genuinely resized and a terminal reflows. `zwlr_layer_shell_v1` is bound at **4**
      — a client bound at 1 asking for `ON_DEMAND` is granted EXCLUSIVE, which would park the
      seat's keyboard on the OSK and stop every window receiving anything.
- [ ] The keyboard takes **no keyboard focus itself** (`keyboard = 0`). It is driven entirely by
      touch and it must never be what the compositor thinks you are typing into.
- [ ] `osk.conf` in skel, PARSED, unknown key reported by name: `osk_mode`, `layout`,
      `height_pct`, `haptic`, `predict`, `cell`, `landscape_rows`, `show_on_focus`.

**Checkpoint**
```sh
kdos-osk --dump --dump-size 33x36 | head -20
```
Expected: a five-row keyboard drawn as cells, with the number row and the modifier row present.

### Task 4.2: The layout format and the shipped layouts

- [ ] `/usr/share/kdos/osk/<name>.layout`, plain text, PARSED never sourced, banner header. One
      row per line; a key is `label:keysym[:shifted[:symbol]]`; `>` prefixes a width multiplier.

```
# hacker.layout — the 5-row PC layout. Parsed, never sourced.
name    = hacker
rows    = 5
row1 = 1:1:exclam:F1  2:2:at:F2  3:3:numbersign:F3  ...  0:0:parenright:F10
row2 = q:q:Q:Tab  w:w:W:Prior  e:e:E:Next  ...  p:p:P:Delete
row3 = a:a:A:Home  s:s:S:End  ...  l:l:L:apostrophe
row4 = >1.5 Shift:Shift_L  z:z:Z:Escape  ...  >1.5 BackSpace:BackSpace
row5 = Ctrl:Control_L  Alt:Alt_L  Fn:Fn  >4 space:space  Left:Left  Down:Down  Up:Up  Right:Right
```

- [ ] Ship `hacker` (the default), `qwerty` (four rows, no number row, for prose), `numeric` and
      `symbols`.
- [ ] **The layout is not chosen per application** (decision 9, rejected). It is chosen by
      `osk.conf`, by the layout key on the keyboard itself, and — the one exception —
      by `text_focus`'s `purpose` field, so a number field gets `numeric`. That is the protocol
      telling the keyboard what kind of field it is, not the keyboard guessing from an app_id.
- [ ] The layout engine computes key rectangles from the row specs, the screen width and the
      floors in Global Constraints, and **refuses to draw with a log line naming the numbers** if
      the key floor and the screen-share ceiling cannot both hold.

**Checkpoint**
```sh
kdos-osk --dump-layout --mm --layout hacker --screen 1080x2340
kdos-osk --dump-layout --mm --layout hacker --screen 2340x1080
```
Expected: portrait 5 rows, every key ≥ 6.0 mm × 8.0 mm, total ≤ 45 %; landscape 4 rows, every key
≥ 6.0 mm × 7.0 mm, total ≤ 50 %; a `FAIL` line naming any key that misses.

### Task 4.3: `virtual-keyboard-v1`, the keymap, and latching modifiers (I7)

- [ ] `keys.c`: build **one xkb keymap covering the whole shipped layout set across four levels**
      and upload it once at startup with `zwp_virtual_keyboard_v1.keymap`. A per-keystroke keymap
      upload (wtype's trick) is correct for arbitrary Unicode and far too heavy for typing; it is
      not used.
- [ ] A character outside that keymap is **refused with a debug line in `keys` mode** and goes
      through the IM path in `im` mode. Saying so beats silently dropping a keystroke.
- [ ] Modifiers are set with `zwp_virtual_keyboard_v1.modifiers()`, which is what makes `Ctrl-C`
      reach `foot` as a real `Ctrl-C` rather than as a character.
- [ ] **Latching, with a visible state**, three-state and drawn differently in each:

  | Tap count | State | Drawn | Cleared by |
  |---|---|---|---|
  | 0 | off | `KT_DIM` fill, `KT_TEXT` label | — |
  | 1 | latched (one shot) | `KT_MID` fill | the next non-modifier key's release |
  | 2 | locked | accent fill | a third tap |

- [ ] **Emphasis is a FILL plus swapped slots, never `KT_A_REVERSE` over a label** — the existing
      rule, and it matters more here than anywhere: reverse video on `Ctrl` lights three cells
      and leaves the key's box dark, which reads as a rendering fault.
- [ ] A key repeats while held, at the seat's own `repeat_info` rate. Key repeat is the CLIENT's
      job on Wayland; the OSK is the client.
- [ ] Fn is a layer, not a modifier: it swaps the labels and the keysyms for as long as it is
      latched, and it is what makes F1–F12 and Home/End/PgUp/PgDn reachable at all.

**Checkpoint** — under the QEMU rig with `foot` running `sleep 100`:
```sh
testing/qemu-mobile/touch.py replay testing/touch/osk-ctrl-c.trace
```
Expected: the `sleep` dies. The trace taps `Ctrl`, sees it latch, taps `c`, and the latch clears.

### Task 4.4: `osk_mode = im` — the input-method path

- [ ] `im.c`: bind `zwp_input_method_manager_v2`, create a `zwp_input_method_v2`, and handle
      `activate` / `deactivate` / `surrounding_text` / `content_type` / `done`. In this mode the
      keyboard commits STRINGS and can show a preedit, which is what the completion strip needs
      to be able to replace a partially typed word.
- [ ] **In `im` mode a `zwp_virtual_keyboard_v1` is created as well** — it is the only route for
      Ctrl, Esc, arrows and the F-keys, none of which are text — and `fcitx5` is not started.
      `kdos-desktop-start` reads `osk_mode` and skips fcitx5 accordingly; **it says so in the
      session log**, because a machine that silently lost pinyin is a bug report about fcitx5.
- [ ] In `keys` mode the input-method manager is **not bound at all**, so fcitx5 keeps it and
      CJK survives. The OSK's keystrokes reach fcitx5 exactly as a physical keyboard's do, which
      is the composition that makes decision 2 work.
- [ ] `show_on_focus` drives raising and lowering from the `kdos-input.sock` stream in `keys`
      mode and from `activate`/`deactivate` in `im` mode. **Both paths converge on one
      `osk_set_visible()`**, or the keyboard develops two different ideas of when it is up.

**Checkpoint**
```sh
kdos-osk --print-mode ; grep -i 'fcitx5\|osk_mode' ~/.local/state/kdos/session.log | tail -5
```
Expected: `keys` reports fcitx5 running and the IM manager unbound; `im` reports fcitx5 skipped,
by name, with the reason.

### Task 4.5: The spacebar trackpad and the flick layers

- [ ] **Spacebar as a trackpad (I8).** A press on the spacebar arms; motion beyond slop stops
      inserting spaces and starts emitting `Left`/`Right` at one keysym per `key_w / 2` pixels of
      travel; the release ends it. With **Shift latched**, the same slide emits `Shift+Left` /
      `Shift+Right` and therefore **extends a selection**.
- [ ] The button is what is remembered, not the motion — Wayland delivers plain motion and
      dragged motion identically, which is the volume slider's rule and the quick-launch
      reorder's rule, arrived at here for the third time.
- [ ] A slide that never exceeds slop inserts one space on release. A spacebar that stopped
      typing spaces because a thumb rolled would be unusable.
- [ ] **Flick layers.** A press on any key followed by motion ≥ slop **vertically** and released
      within the key's own column emits the shifted keysym (up) or the symbol keysym (down)
      instead of the base one, and never the base one as well. Horizontal motion off a key is a
      drag across the keyboard and selects the key under the finger, which is how every phone
      keyboard has worked since 2007.
- [ ] The flick is what puts the number row's contents within reach in landscape, where there is
      no number row. It is therefore load-bearing there and a convenience in portrait.
- [ ] A key preview — the character drawn one row above the finger while it is down — is
      **suppressed for the top row**, where it would be off the surface, and for the flick
      layers, where the finger is already showing the answer by moving.

**Checkpoint**
```sh
testing/qemu-mobile/touch.py replay testing/touch/osk-space-caret.trace
testing/qemu-mobile/touch.py replay testing/touch/osk-flick-symbol.trace
```
Expected: the caret moves without a space being typed; the flick produces `!` from the `1` key
and no `1`.

### Task 4.6: The completion strip

- [ ] `predict.c`: a trie over `/usr/share/kdos/osk/words.<lang>` plus
      `$XDG_STATE_HOME/kdos/osk-history`, ranked by wordlist frequency and by the user's own
      counts. The history file is written temp + `fsync` + rename like every other state file in
      this tree.
- [ ] **Three candidates, one row (two rows on the touch floor), and the strip NEVER rewrites.**
      A candidate is inserted only when it is tapped. There is no autocorrect, no
      correct-on-space and no silent substitution, and that is a policy — a keyboard that
      second-guesses `nft`, `HEAD~3` or a passphrase is a keyboard that cannot be used for work.
- [ ] **When the focused window is a terminal the strip becomes the hacker row instead** — Esc,
      Tab, Ctrl, Alt, the four arrows and Fn, permanently visible. The focused `app_id` arrives
      on `kdos-input.sock`; the terminal set is a config list (`terminal_apps` in `osk.conf`),
      defaulting to `foot`, and NOT a hardcoded string.
- [ ] The strip is a **fixed-width three-slot field**. A strip that sized itself to its
      candidates would move the whole keyboard's geometry between keystrokes, which is the same
      defect the panel's fixed-width applet tile exists to prevent.
- [ ] `predict = no` in `osk.conf` removes the strip entirely and gives its rows back to the
      keys.

**Checkpoint**
```sh
kdos-osk --dump --dump-size 33x36 --focus firefox-esr | head -4
kdos-osk --dump --dump-size 33x36 --focus foot        | head -4
```
Expected: three candidate slots in the first; the hacker row in the second; the same total height
in both.

### Task 4.7: Haptics

- [ ] `haptic.c`: a short pulse on key-down and on long-press commit, written to the vibrator's
      sysfs or its force-feedback evdev node. **Verify which fajita exposes; do not recall.**
- [ ] `fs/etc/udev/rules.d/70-kdos-haptic.rules` grants the node to the `input` group, and `kdos`
      is already in the groups the seat needs. **Both halves are required** — the rule alone has
      no group to grant to and the membership alone grants nothing, the lesson
      `70-kdos-*.rules` already carries. `MODE="0660"`, never 0666.
- [ ] The write is **fire-and-forget and never on the frame path**. A keyboard that stalls a
      frame to buzz is worse than one that does not buzz.
- [ ] `haptic = no` in `osk.conf`, and a machine with no vibrator logs once at startup and never
      again.
- [ ] `kdos doctor` gains a Hardware line for the vibrator, with the `skip` level and a reason
      when there is none — a VM has no vibrator and reporting `ok` or `warn` would both be wrong.

**Checkpoint**
```sh
kdos doctor 2>&1 | grep -i -A1 haptic
```
Expected on the device: `ok` naming the node. Expected in a VM: `skip` with the reason.

### Task 4.8: Dumps, goldens and the key-size audit

- [ ] `--dump` and `--dump-cells` for every layout, at **33x36** and **73x16**, into
      `testing/goldens/mobile-osk-*`.
- [ ] `--dump-layout [--mm]` prints one line per key — name, x, y, width, height, in cells and
      in millimetres — and exits non-zero if any key misses its floor.
- [ ] `testing/preflight.sh` runs `--dump-layout --mm` for every shipped layout at both
      orientations. **This is the check that catches a key nobody can hit, and it runs in
      seconds with no VM** — which matters, because the touch behaviour itself cannot.
- [ ] `testing/selftest.sh` diffs the goldens.

**Checkpoint**
```sh
testing/preflight.sh 2>&1 | grep -i osk
testing/selftest.sh   2>&1 | grep -i 'mobile-osk'
```
Expected: every layout passes the floor at both orientations; every golden matches.

---

## Wave 5 — `kdos-mobile`, the chrome

**Gate: the phone is usable with no keyboard and no cable — launch, switch, close, read a
notification, change a setting, open a file.**

The shape, decided in Global Constraints: **swipe from any edge to leave an app** (Nokia N9
Harmattan), **a card stack you flick away to close** (Palm webOS), and **a permanent one-row
soft-key strip with two per-app labels** (Symbian S60 / BlackBerry) so that the gestures are
discoverable at all. The last is the part every modern gesture-navigation phone gets wrong.

### Task 5.1: One application index, one favourites writer — the refactor

This lands **before** any mobile surface, because the alternative is two shells with two answers.

- [ ] Move `src/desktop/kdos-shell/apps.c` into **`src/libs/libkxdg/kxdg_apps.c`** as
      `kxdg_app_index_load/free/find/search`, carrying its `NoDisplay` rules, its `alien` flag
      (derived from an `Exec` that is the box launcher), its `StartupWMClass` second pass and its
      usage-count reader verbatim. `kdos-shell` includes the header and loses the file.
- [ ] Move `src/desktop/kdos-shell/fav.c` into **`src/libs/libkchrome/kch_fav.c`** as
      `kch_fav_load/set/move`, keeping the temp + `fsync` + rename write. `~/.config/kdos/favorites`
      keeps exactly one writer, which was the point of `fav.c` existing.
- [ ] Move `sh_desktop_entry` — the `app_id` → desktop-entry resolution, ID first then
      `StartupWMClass` — into `kxdg_apps.c` beside the index it queries.
- [ ] **`kdos-shell` must be byte-identical in behaviour.** The proof is its own goldens.

**Checkpoint**
```sh
testing/selftest.sh 2>&1 | grep -E 'golden|shell'
nm build/fs/usr/bin/kdos-shell | grep -c apps_index   # the symbol comes from the library now
```
Expected: every existing `kdos-shell` golden still matches; no duplicate index symbol.

### Task 5.2: The package, `kdos-bar` and `kdos-nav`

- [ ] NEW `src/desktop/kdos-mobile/` with `kpkgbuild`, `build.sh`, `mobile.h`, `main.c` and a
      `TOOLS[]` table dispatched on basename — the `kdos-shell` shape exactly. **A name in
      `TOOLS[]` with no `<name>_main` is a link error; a name `build.sh` does not symlink is a
      program nothing can reach.** Both halves are one edit.
- [ ] Names: `kdos-bar`, `kdos-nav`, `kdos-home`, `kdos-cards`, `kdos-shade`, `kdos-sheet`,
      `kdos-sel`.
- [ ] **`kdos-bar`** — the status bar. One row, top edge, exclusive zone. Carries the clock, the
      battery, the network, the privacy indicators (`●MIC` / `●CAM`, reusing `privacy.c`'s two
      sources unchanged) and the unseen-notification count from `kdos-notifyd`'s existing ring.
      It is **one row, not two**: the desktop's second row is a detail line and a phone has no
      room for one.
- [ ] **`kdos-nav`** — the bottom strip, one touch-target tall, exclusive zone. Two soft-key
      labels whose text comes from the focused window (`Menu` / `Close` by default; an
      application may name its own through the command socket) and a centre affordance that is
      the visible target for `edge_up`.
- [ ] Both are **per-output** in the compositor's template set (Task 6.2), like `kdos-shell` is.
- [ ] Everything the desktop panel already learned applies verbatim and is not re-derived here:
      the hit map is recorded from what was DRAWN; an applet with no room records an EMPTY span;
      hover is a FILL and never `KT_A_REVERSE`; a label is `KT_MID`, never `KT_DIM`; the poll
      deadline is shortened to whatever the next animation needs.

**Checkpoint**
```sh
kdos-bar --dump --dump-size 33x1
kdos-nav --dump --dump-size 33x2
```
Expected: the bar fits 33 columns with the clock, battery and count all present; the nav strip
draws two labels and a centre affordance, each on the touch floor.

### Task 5.3: `kdos-home` — four panes

- [ ] A `KWL_ROLE_BACKGROUND` surface covering the output between the bar and the nav strip.
      Background rather than overlay: reserving space would shrink the usable box for every
      window, which is the same reason `kdos-desk` is a background surface.
- [ ] Four panes, swiped horizontally, with page dots. **The pane is a viewport offset, not four
      surfaces** — libktui has one cell buffer per process and a second surface would be a second
      process.
- [ ] **Events** — the unseen notifications from `kdos-notifyd`'s socket (`count` / `list`), the
      battery, the clock, and the CPU/RAM area graphs drawn by the existing `libkproc` +
      `kch_tile` code. Nothing here is new drawing; it is the meters strip at a phone's scale.
- [ ] **Apps** — a search field and a grid of `ktui_touch_min_*`-sized tiles from
      `kxdg_app_index`. `[box]` is marked on every app whose `Exec` is the box launcher, from
      `kxdg_app.alien` — the same source the desktop's Start menu reads, so the two cannot
      disagree about which apps cost a container start.
- [ ] **Running** — the card stack, drawn by `kdos-cards` (Task 5.4) inline in this pane, and by
      the same code when raised as a full surface by `edge_up_hold`.
- [ ] **Console (I6)** — a `foot` window kept alive on a dedicated workspace and raised by
      swiping to this pane. `foot -e tmux new-session -A -s kdos`, so the session survives the
      window being closed, the compositor restarting and the phone rebooting into it. **The pane
      shows the window; it does not draw a terminal** — a cell grid drawing another cell grid's
      contents would be a second terminal emulator.
- [ ] The search field searches applications **and** the fixed rows of the settings and places
      the way the desktop's Start menu does, with per-row synonyms, because somebody typing
      `wifi` has stopped looking at the quick-settings grid.
- [ ] The last pane is remembered in `$XDG_STATE_HOME/kdos/mobile-pane`, written the
      fsync-rename way.

**Checkpoint**
```sh
for p in events apps running console; do kdos-home --dump --dump-size 33x33 --pane $p; done
kdos-home --dump --dump-size 73x13 --pane apps
```
Expected: four panes, each fitting both sizes, no row crossing the frame, page dots present.

### Task 5.4: `kdos-cards` — the card switcher (I5)

- [ ] Cards come from `wlr-foreign-toplevel`, which `kdos-shell` already binds; the name is the
      desktop entry's `Name`, resolved through `kxdg_apps` — `Name` > title > `app_id` — because
      `org.gnome.Meld` is not a human name.
- [ ] **Each card carries an ASCII-art thumbnail of its own window**, rendered with
      `kcell_ascii_image` from a per-window capture. `ext_image_copy_capture` with the
      foreign-toplevel source is what the fork already exposes and what
      `xdg-desktop-portal-wlr` prefers; `cycle/osd-thumbnail.c` already renders a view's scene
      tree if a direct render is preferred.
- [ ] `cards = ascii | pixel` in `mobile.conf`. **ASCII is the default** — it is what this
      desktop looks like — and `pixel` draws the capture as a sprite for anyone who wants the
      real picture.
- [ ] **A capture is taken at most once per second per card and never on the frame path.** A
      switcher that stalls to screenshot six windows is a switcher that feels broken. A card
      with no capture yet draws its icon and its name, which is a complete card.
- [ ] **Flick a card up to close it** (webOS). The close is `foreign_toplevel.close` — polite, so
      an editor still gets to ask. Tap to raise. Long-press opens the window menu (Restore /
      Minimize / Maximize↔Restore Down / Fullscreen / Close), reading the toplevel's own state,
      exactly as the desktop's window menu does.
- [ ] **Drag a card to a screen edge to tile it** in landscape — the split-screen half of
      decision 10. labwc already tiles; the mobile part is the drag and the drop target.

**Checkpoint**
```sh
kdos-cards --dump --dump-size 33x33 --fixture testing/fixtures/mobile/toplevels
kdos-cards --dump --dump-size 73x13 --fixture testing/fixtures/mobile/toplevels
```
Expected: cards with names and ASCII thumbnails at both sizes; the landscape dump shows the two
tile drop targets.

### Task 5.5: `kdos-shade` — notifications and quick settings

- [ ] Raised by `edge_top_pull`, an overlay anchored to the top with **`exclusive_zone = -1`**
      so the caller's margin is the only offset. Dragged down to open, up or tapped away to
      dismiss.
- [ ] The notification list is `kdos-notifyd`'s ring over its existing socket (`count` / `list` /
      `seen` / `open` / `forget` / `clear` / `dnd`). **Nothing is re-derived here** — the daemon
      owns the list and a front end draws it, which is the split `kdos-clip` and `kdos-notify`
      already use. Opening the shade sends `seen`, which is what clears the bar's badge, and
      nothing else clears it.
- [ ] Quick settings: a grid of touch-floor toggle tiles — Wi-Fi, Bluetooth, rotation lock, Do
      Not Disturb, torch, airplane, brightness, volume. Each **shells out to the program that
      already owns it** (`kdos-net`, `kdos-bt`, `kdos-audio`, `kdos-notify dnd`) rather than
      reimplementing it; a tile that could not reach its program says so on the tile.
- [ ] Brightness and volume are sliders, dragged, and the **press arms / motion tracks / release
      decides** rule applies — the volume slider's rule for the fourth time in this tree.

**Checkpoint**
```sh
kdos-shade --dump --dump-size 33x20
kdos-notify count   # before and after opening the shade
```
Expected: the notification list and the toggle grid both drawn; the unseen count goes to zero
only after the shade is opened.

### Task 5.6: `kdos-sheet` — the dialog shape

- [ ] The mobile modal is a **bottom sheet**, not a centred dialog: it rises from the edge the
      thumb is already at, and it is dragged down to dismiss.
- [ ] `kdos-sheet` is the front end over `kch_sheet` for the callers that need a dialog as a
      separate process — a confirmation, a picker, a chooser. It answers with an **exit status**
      the way `kdos-prompt` does (0 yes, 1 no, 254 cancelled), so `<promptCommand>` and every
      existing caller work unchanged.
- [ ] `kdos-pick`, `kdos-openwith` and `kdos-prompt` gain a mobile presentation through
      `kch_sheet` **without a second implementation** — the same programs, laid out for the
      floor, chosen by `kdos-mode`.

**Checkpoint**
```sh
kdos-sheet --dump --dump-size 33x14 --title 'Delete file?' --buttons 'Delete,Cancel'
kdos-pick  --dump --dump-size 33x36 --mobile
```
Expected: a sheet on the bottom edge with buttons on the touch floor; `kdos-pick` in sheet form
with its hint row and its buttons on their row without colliding — the check
`testing/selftest.sh` already makes for the desktop chooser.

### Task 5.7: `kdos-sel` — cell-granular selection over anything (I2)

**This is the feature no other platform can have, and it is the one that makes reading a build
log on a phone actually work.**

- [ ] `longpress_2` (two-finger long-press, from `gestures.conf`) raises `kdos-sel`: a
      transparent overlay covering the output that captures the screen underneath, snaps a
      selection rectangle to the **cell grid**, and lets it be dragged by its corners.
- [ ] The text is recovered **from the cells**, not from OCR: for a KDOS surface the shell can
      ask for the cell contents; for anything else the capture is passed through
      `kcell_ascii_image`'s inverse — the glyph the cell most closely matches — and the result is
      marked as best-effort in the sheet that offers it. **Say which of the two happened**, so a
      user knows whether they have the real characters or an approximation.
- [ ] `[ Copy ]` puts it on the clipboard through `kwl_copy`. A `wl_data_source` is destroyed on
      `cancelled`, never at set time — destroying it when the selection is made cancels the
      selection just made.
- [ ] A selection needs a serial. `set_selection` presents the serial of the event that justified
      it and a compositor refuses one it has never issued; the overlay has just had a touch, so
      it has one. **A background client cannot take the clipboard** and that is the protocol, not
      a bug.

**Checkpoint**
```sh
testing/qemu-mobile/touch.py replay testing/touch/sel-rect.trace
kdos-clip --dump | head -3
```
Expected: the rectangle appears, snaps to cells, and the copied text is the top entry in the
clipboard history.

### Task 5.8: Lock, rotation and the hardware keys

- [ ] **`kdos-lock --pin`**: a PIN pad on the touch floor, feeding the existing
      `kdos-checkpass` — which takes NO arguments, checks the caller's own uid, reads the
      password on **stdin** because `/proc/<pid>/cmdline` is world-readable, and returns 0/1/2
      that the caller must distinguish. None of that changes; the pad is a new front end on it.
- [ ] The lock screen shows notifications from `kdos-notifyd`'s ring, honouring Do Not Disturb
      and showing an urgent one anyway — the daemon's existing policy, not a second copy of it.
- [ ] **`kdos-rotate.c`** graft: the IIO accelerometer under `/sys/bus/iio/devices/` drives the
      output transform. **Hysteresis and a dead zone are mandatory** — a threshold with neither
      flaps at 45°. A rotation is committed only after the new orientation has held for 500 ms.
      `rotate = auto | lock | 0 | 90 | 180 | 270` in `comp.conf`, live-reloaded, and the quick
      settings tile writes it.
- [ ] **Rotation is a resize**, which is the cell grid paying off again: every surface re-lays
      out through the path Task 2.3 already exercises, and 33x36 becomes 73x16 with no
      orientation-aware code anywhere in the chrome.
- [ ] Power and volume keys arrive as ordinary evdev keys (`KEY_POWER`, `KEY_VOLUMEUP/DOWN`) and
      are bound in `rc.xml`, not in a graft: short power press → screen off and lock, long press
      → `kdos-prompt` power menu, volume keys → `kdos-osd`. **`<default />` stays the first child
      of `<keyboard>`.**
- [ ] Verify **on the device** which of `KEY_POWER` and a PMIC long-press is handled in firmware;
      a hardware force-off at 10 s is not something userspace can or should fight.

**Checkpoint**
```sh
kdos-lock --pin --dump --dump-size 33x36
kdos hey output DSI-1        # transform follows the accelerometer
```
Expected: a PIN pad on the floor with notifications above it; the transform changes with the
device and holds steady at 45°.

### Task 5.9: `mobile.conf`, and the mobile defaults

- [ ] `~/.config/kdos/mobile.conf`, PARSED, unknown key reported by name, shipped in skel fully
      commented. Keys: `panes`, `cards`, `bar_widgets`, `nav_labels`, `search_synonyms`,
      `console_command`, `tile_landscape`.
- [ ] `bar_widgets` is a LIST, exactly as the desktop panel's `right =` is, and an unknown
      widget name is **reported, not ignored**. That is the whole widget system on mobile too;
      the extension point for anything else is a gesture bound to a command.
- [ ] The shell re-reads it on **SIGHUP**, the signal `kdos theme` already sends, so a settings
      change lands on the bar that is on the screen. `load_widgets()` restores every default
      before parsing, because a reload that only ever ADDED would leave a widget hidden after
      the line hiding it was deleted — the desktop panel's rule, arrived at once.
- [ ] **`kdos-mobile`'s surfaces install a SIGHUP handler.** The desktop learned this the
      expensive way: `kdos-shell` had none, the signal killed the panel, the supervisor
      respawned it, and six accent changes in twelve seconds tripped `RESPAWN_MAX` and lost the
      panel for the session.

**Checkpoint**
```sh
kdos theme amber && sleep 1 && kdos hey list      # the bar and nav pids are UNCHANGED
```
Expected: the chrome retints live and no pid changed.

---

## Wave 6 — Convergence

**Gate: one phone, two screens, two shells, at the same time.** Plug the phone into a monitor
and a keyboard and the monitor shows the full desktop taskbar with `kdos-desk`'s icons while the
phone screen keeps the mobile chrome and the on-screen keyboard steps out of the way.

**Named risk, carried openly:** USB-C **DP alt-mode on SDM845 mainline is unverified** in the
kernel this tree builds. Tasks 6.1–6.3 and 6.5 are therefore proven on **QEMU with two outputs**
and need no phone at all; only Task 6.4 needs the hardware, and the arc lands with or without it.
If DP alt-mode turns out to be absent, this wave still delivers per-output chrome, the docked
profile and window fitting — every one of which is independently useful — and the fallback is
stated rather than discovered.

### Task 6.1: `kdos-mode.c` — what kind of thing is this screen

- [ ] NEW graft classifying **each output**: internal or external (from the connector type —
      `DSI`/`eDP` are internal, `DP`/`HDMI` are not), touch-capable (an input device mapped to
      it), and physically small or large (`wl_output.geometry`'s millimetres, with the diagonal
      computed the way Global Constraints does it).
- [ ] And **each seat**: is a physical keyboard present, is a pointing device present.
- [ ] It publishes a `mode` event on `kdos-input.sock` (Task 3.3) on every change, and answers a
      `mode` query on `kdos-cmd.sock` for a caller that wants the current state rather than a
      stream. **Two sockets, two purposes**, which is the distinction that file already draws.
- [ ] Three forms, and the boundaries are config, not magic numbers buried in code:
      `handset` (internal, touch, diagonal < 180 mm), `tablet` (internal, touch, larger),
      `desk` (external, or no touch). `form_override` in `comp.conf` forces one, because a
      classification that cannot be overridden is a classification that is wrong on somebody's
      hardware.

**Checkpoint**
```sh
kdos hey mode
```
Expected, under the two-output QEMU rig: one `handset` and one `desk` row, with the evidence
each was classified on.

### Task 6.2: A chrome template SET per output (I3)

- [ ] `kdos-child.c`'s `TEMPLATES[]` becomes **two sets**, selected per output by
      `kdos_mode_form()`:

  | form | chrome spawned on that output |
  |---|---|
  | `handset` / `tablet` | `kdos-bar`, `kdos-nav`, `kdos-home`, `kdos-osk` |
  | `desk` | `kdos-shell`, `kdos-desk`, `kdos-slit` |

- [ ] Session-wide singletons stay singletons: `kdos-notifyd` owns a bus name and a second
      instance would simply fail to take it; `kdos-clip` is one clipboard.
- [ ] Every existing per-output rule survives verbatim: one set per output spawned from the
      output-created hook, `SIGTERM` and a `stopping` mark on output-destroyed so the reap frees
      the slot instead of respawning into a screen that is gone, `RESPAWN_MAX` at five deaths in
      thirty seconds, `child_reset_signals()` before every `execvp` — a supervised child gets
      every ignored disposition back, not just SIGPIPE.
- [ ] **`kdos-osk` hides when a physical keyboard appears on the seat and returns when it
      leaves**, and it is a hide, not a kill: a keyboard that had to be restarted would lose its
      latch state and its history mid-sentence.
- [ ] A `form` change on a LIVE output tears down that output's chrome and spawns the other set.
      **This is the only place chrome is replaced rather than respawned**, and it is marked
      `stopping` first for the same reason an output-destroy is.

**Checkpoint**
```sh
testing/qemu-mobile/run.sh --outputs 1080x2340,1920x1080
kdos hey list | grep -E 'kdos-(bar|nav|shell|desk)'
```
Expected: `kdos-bar` and `kdos-nav` on the first output, `kdos-shell` and `kdos-desk` on the
second, all four alive at once.

### Task 6.3: `kdos-fit.c` — oversized windows (I10)

- [ ] NEW graft. A toplevel whose **minimum** size exceeds the output is configured at a virtual
      size (default 1920x1080) and its scene buffer is scaled to fit the output's width with
      `wlr_scene_buffer_set_dest_size()` — used in five places in this fork already, so the
      mechanism is established rather than invented.
- [ ] `window_fit = scale | pan | off`, global in `comp.conf` and overridable **per `app_id`**,
      so a well-behaved application is never touched. Default `scale`, and `off` for anything
      that fits.
- [ ] **Pinch zooms into the scaled window and a two-finger drag pans it** — `KT_G_PINCH` and
      `KT_G_PAN2` from Task 2.3, consumed here. A window at 1.0 scale pans not at all, which is
      the honest state and needs no special case.
- [ ] The pointer position must be transformed by the same scale on the way IN, or every click
      on a fitted window lands somewhere else. **This is the failure mode of every naive
      implementation of this idea** and it is one function used by both directions.
- [ ] A fitted window's `wl_output` scale and its `xdg_toplevel` bounds are reported as the
      VIRTUAL ones, so the application lays out for the size it was given rather than for the
      screen it cannot see.

**Checkpoint**
```sh
kdos hey window active                       # reports virtual 1920x1080, fit scale 0.56
testing/qemu-mobile/touch.py tap 900 1900    # lands on the OK button of a 1600-px-wide dialog
```
Expected: the dialog's OK button is reachable and the tap lands on it, not beside it.

### Task 6.4: DP alt-mode and the dock — **MANUAL, needs the device**

- [ ] Verify against the sdm845-mainline tree — **read it** — whether the fajita device tree and
      the KDOS kernel fragment carry a working USB-C DP alt-mode path (`qcom,pmic-typec`, the
      `usb-c-connector` graph, `msm_dp`). **Record the finding either way**; a negative is a
      result, not a failure.
- [ ] If present: bring up an external display over a USB-C hub, confirm two `wl_output`s, and
      run Task 6.2's checkpoint on real hardware.
- [ ] If absent: **say so in this task and in `CLAUDE.md`**, keep the QEMU proof as the arc's
      evidence, and name the kernel work as a later arc. Do not work around it and do not leave
      the reader to discover it.
- [ ] Either way: an external keyboard and mouse over the hub, `kdos-osk` hiding, and
      `window_memory` restoring window positions per dock state.

**Checkpoint** — pasted in by hand:
```
DP alt-mode on fajita: <present|absent>  evidence: <dts node / kernel log / commit>
external display: <name> <WxH> <mm x mm>   outputs seen by kdos hey: <n>
external keyboard: <name>  kdos-osk hidden: <yes|no>
```

### Task 6.5: The CRT pass at 402 dpi, and on a battery (I9)

- [ ] `kdos-crt.c`: the scanline period becomes **3 × the output scale** rather than 3 physical
      rows. At scale 2 that is a 6-px stripe = 0.38 mm — visible — where 3 physical rows is
      0.19 mm and below what the eye resolves. `crt_scanline_period = auto | <n>`; `auto` is the
      new default and an explicit number still wins.
- [ ] `crt_battery_floor = <percent>`, default 20. Below it, and not while charging, the pass
      drops to the phosphor floor alone — no scanlines, no bleed, no curve. The battery state
      comes from `/sys/class/power_supply`, read on a slow timer and **never on the frame path**.
- [ ] Mobile defaults in the shipped `comp.conf`: `crt = 45`, `crt_scanlines = 55`,
      `crt_curve = 8`, `crt_fullscreen` unchanged.
- [ ] Every existing rule stands: direct scanout stays off for the session while the pass is on
      (`kc_crt_init()` before `wlr_scene_create()`); the texture is imported per frame and
      destroyed after (caching it per swapchain slot deadlocks the swapchain — measured, not
      reasoned about); a non-GLES2 renderer gets no pass and says so at startup; a runtime
      failure marks that output broken for good rather than logging sixty times a second; and
      the pass declines while the magnifier is enabled.
- [ ] **Verify the shader compiles and runs on freedreno**, which is a GLES2 stack this pass has
      never met. `KDOS_CRT_DUMP=<prefix>` writes the input and the output as PPMs and is how it
      is looked at without a screen.

**Checkpoint**
```sh
KDOS_CRT_DUMP=/tmp/crt KDOS_CRT_DUMP_FRAME=8 make run-mobile-shot BOARD=qemu-aarch64
head -2 /tmp/crt-in.ppm /tmp/crt-out.ppm
```
Expected: both PPMs written at 1080x2340; the scanline period measurable at 6 px in the output;
a battery below the floor produces an output with the floor only.

---

## Wave 7 — Wi-Fi

**Gate: the phone reaches the network with no cable.** A workstation that needs a USB tether is
not a workstation. Telephony remains out (see *What is explicitly NOT in this arc*); this wave
brings up the parts Wi-Fi needs, which happen to be most of the parts a modem would also need.

### Task 7.1: `qrtr`, `pd-mapper` and `rmtfs`

- [ ] Three new ports under `ports/mobile/`. **Every version, URL and sha256 is read from
      pmaports and upstream at the moment the recipe is written, never recalled**, and recorded
      in the recipe's `sha256 =` key so `testing/preflight.sh` checks the archive.
- [ ] `qrtr` is the Qualcomm IPC router userspace; `pd-mapper` serves protection-domain requests;
      `rmtfs` serves the modem's remote filesystem from the device's own `modem` partition.
      **`rmtfs` reads a partition this plan is otherwise forbidden to write** — `mobile.plan.md`
      hard rule 2 lists `modem` as off limits — and the recipe and the init script must both say
      read-only, explicitly.
- [ ] Init scripts in `fs/etc/init.d/` on the numeric-prefix convention, each sourcing
      `service_helper` and running its daemon **in the foreground** under `supervise`. Ordering:
      `qrtr` before `pd-mapper` and `rmtfs`, all three before `30_network`.
- [ ] **A daemon that cannot do its job here is SKIPPED before `supervise` sees it** — the rule
      `56_energyd.sh` and `57_oomd.sh` already encode. On a board with no Qualcomm IPC these
      three skip with a reason; a refusing daemon under a respawn loop is a boot that never
      settles.

**Checkpoint**
```sh
testing/preflight.sh 2>&1 | grep -E 'qrtr|pd-mapper|rmtfs'
```
Expected: three ports resolve, three recipes carry `sha256`, three init scripts parse, and the
QEMU board skips all three with a reason.

### Task 7.2: ath10k and the wcn3990

- [ ] **Verify against pmaports `firmware-oneplus-sdm845` and `linux-firmware`** which of the
      wcn3990 firmware files fajita needs and where they live. The firmware-lives-under-`enchilada`
      trap from `mobile.plan.md` applies here too and must not be "fixed".
- [ ] Confirm the kernel fragment carries `CONFIG_ATH10K_SNOC` and the QMI transport.
- [ ] The firmware is fetched by the existing host-side firmware step and **never committed** —
      `mobile.plan.md` hard rule 1.

**Checkpoint** — **MANUAL**, on the device:
```
ip link                       # wlan0 present
iw dev wlan0 scan | head      # APs seen
dmesg | grep -i ath10k        # firmware version loaded
```

### Task 7.3: NetworkManager on the phone, and `kdos-net` for a finger

- [ ] NetworkManager, `wpa_supplicant` and `polkit` in the mobile package set, configured exactly
      as the desktop's are — `polkit=true`, internal DHCP, `iptables=` empty. No second policy.
- [ ] **`/var/run` must be a symlink to `/run` on the mobile rootfs too.** `sd_bus_open_system()`
      in basu defaults to the pre-2011 path; a real empty `/var/run` directory means every sd-bus
      client fails to reach the system bus, and `kdos-net` says "NetworkManager is not reachable"
      with NetworkManager running two processes away. `01_phase1/00_file_system.sh` does this on
      the desktop tree; the mobile tree needs the same.
- [ ] `kdos-net` gains the mobile presentation through `kch_sheet` and the touch floor — **the
      same program**, not a second one. Its existing rules stand: the list does not reorder under
      the pointer, the selection follows the SSID rather than the index, and there is still no
      `SecretAgent`, so 802.1X and OTP VPNs still need `nmtui`.
- [ ] The quick-settings Wi-Fi tile opens it as a sheet; typing the passphrase raises `kdos-osk`
      through the ordinary `text_focus` path, which is the end-to-end proof that Waves 3, 4 and 5
      compose.

**Checkpoint** — **MANUAL**, on the device: join a WPA2 network from the quick-settings tile,
with the passphrase typed on the on-screen keyboard, and `ping` a host.

---

## Wave 8 — The arm64 appbox

**Gate: a boxed application launches from the mobile launcher and every one of its controls is
reachable.** Decisions 11 and 12: the image is curated to what makes sense on a phone, and
everything in it is reachable from the phone screen — no docked-only gating.

### Task 8.1: `Containerfile.mobile`

- [ ] NEW `ports/appbox/Containerfile.mobile`, `--platform linux/arm64`, debian trixie, one `RUN`
      per segment so editing a segment rewrites one layer's blob in git — the shape the x86_64
      Containerfile already has and the reason it has it.
- [ ] **Roughly 25 applications, and every one is verified to exist for arm64 in trixie at bake
      time.** Debian is not uniformly arm64 and an assumed package fails the bake. The list is
      decided in the file, not here, but the shape is: a browser, a mail client, a document
      viewer, an office writer and spreadsheet, an image viewer and editor, a media player, a
      password manager, an archiver, a file manager, a text editor, a terminal, a torrent client,
      a remote-desktop client and a calculator. **Not** kicad, freecad, blender, kdevelop,
      digikam or the CAD and EDA segments — they are desktop-scale tools and they are most of the
      image's size.
- [ ] `--no-install-recommends` everywhere; `/.containersetupdone` pre-baked so distrobox-init
      never apt-gets on first enter.
- [ ] The `kdos.qt-kde-theme` / `kdos.qt-gtk-theme` labels are declared in the same layer that
      installs each, so `kdos-appbox`'s one-inspect-per-label-per-boot works identically here.

**Checkpoint**
```sh
make fetch-apps ARCH=arm64
podman image inspect kdos-apps:arm64 --format '{{.Labels}}'
```
Expected: the image builds, carries both labels, and is roughly 1.2 GB.

### Task 8.2: `image-arm64/` — the in-tree chunks

- [ ] `kdos-appbox image pack` explodes the tar into `ports/appbox/image-arm64/`: one zstd file
      per docker-archive member, layer blobs split at 1.5 G, all LFS-tracked, plus `INDEX.json` —
      the same format, verified to round-trip both ways when it was written.
- [ ] `script-mobile`'s packaging phase loads the tar if present, else streams it out of the
      chunks with `kdos-appbox image assemble`. **A missing image is a warning; the boot image
      still builds.**
- [ ] `.gitattributes` tracks the new chunks in LFS. **Repo grows from roughly 5 GB to roughly
      6.2 GB** and KDOS property #3 — clone, `make build`, get an image, no network — survives
      for the mobile target too.

**Checkpoint**
```sh
git lfs ls-files ports/appbox/image-arm64/ | wc -l
du -sh ports/appbox/image-arm64/
```

### Task 8.3: podman on aarch64

- [ ] `podman`, `crun`, `conmon`, `netavark`, `aardvark-dns`, `fuse-overlayfs`, `slirp4netns` and
      `distrobox` into the mobile package set.
- [ ] **Every bake-time trap from the desktop applies verbatim** and each already cost a debug
      cycle: wipe `$STORAGE` before loading; drop root's runroot and tmpdir from the user's
      libpod database after the load, or every rootless podman call fails silently on
      `mkdir /run/libpod`; keep the uid remap idempotent; **flatten the loaded image to one
      layer**, because the rootful unpack records whiteouts as `trusted.overlay.*` which the
      rootless mount cannot see.
- [ ] **The storage-driver choice is the phone's own.** `/etc/containers/storage.conf` pins
      `fuse-overlayfs`; `kdos-appbox` writes a one-time user `storage.conf` with the native
      driver when `$HOME`'s filesystem is ext4/btrfs/xfs **and only while the store has no
      containers yet** — fuse and native write incompatible whiteout formats, so the choice must
      never flip. On a flashed phone `$HOME` is on real UFS, so this is the native path, and it
      matters more here than on a desktop.
- [ ] `kdos-appbox warmup` at login, nice 10, flock-guarded against `run`. Measure the cold and
      warm launch on the device and record them the way the desktop's 18.3 s / 0.3 s / 0.55 s are
      recorded — **a number measured on a phone, not the desktop's number reused.**

**Checkpoint** — **MANUAL**, on the device:
```
kdos-appbox run <app>            # cold
kdos-appbox run <app>            # warm
cat $XDG_RUNTIME_DIR/kdos-appbox.trace
```

### Task 8.4: The launcher, the fit, and the honest cost

- [ ] `kdos-appbox genlaunchers` runs against the arm64 image and writes the same four outputs —
      the launcher named with **upstream's own desktop id**, `mimeinfo.cache` beside it,
      `usr/share/kdos/alien-apps`, and a `usr/local/bin/<name>` shim. Dropping any one breaks
      something visible.
- [ ] `kxdg_apps` marks them `alien`, so `kdos-home`'s Apps pane shows `[box]` from the same
      source the desktop's Start menu reads.
- [ ] `window_fit` (Task 6.3) is exercised on the three widest applications in the image, and
      **the results are recorded in this task** — which fitted cleanly, which needed panning, and
      which is genuinely awkward on a 1080-wide screen.
- [ ] **State the cost.** This is the decision the plan flagged: a desktop-scale application on a
      phone screen is reachable but not comfortable, and `kdos doctor` or the launcher tile is
      the honest place to say which applications are better docked. Naming them beats letting the
      user find out.

**Checkpoint**
```sh
kdos-home --dump --dump-size 33x33 --pane apps | grep -c 'box'
```
Expected: every boxed application marked, and the three widest named in this task's notes with
their measured fit behaviour.

---

## Wave 9 — Preflight, goldens, docs and the device acceptance matrix

**Gate: the wiring is checked in seconds, the layouts are checked without a screen, and what is
true about this desktop is written down where the next reader will find it.**

### Task 9.1: `testing/preflight.sh` additions

Everything a full build would catch, minus the build — the file's existing charter, extended to
the two new packages. Each check below is a defect that has already shipped in this tree in its
desktop form.

- [ ] **Every flag one `kdos-mobile` or `kdos-osk` tool passes another is one the other
      accepts.** This is the check three dead controls needed on the desktop: the panel spawned
      `kdos-cal --at-bottom X Y` for a release and `kdos-cal` had never accepted `--at-bottom`,
      so clicking the clock did nothing, silently, and no compile and no golden could see it.
      The mobile chrome spawns six programs by name and the same trap is waiting.
- [ ] **Every command named in the shipped `gestures.conf`, `osk.conf` and `mobile.conf` is
      provided by the tree** — the check `<promptCommand>labnag</promptCommand>` needed.
- [ ] **Every layout file parses** and every keysym in it resolves against the shipped xkb
      keymap. A keysym that does not resolve is a key that does nothing, silently.
- [ ] **`kdos-osk --dump-layout --mm` passes for every shipped layout at both orientations.**
      This runs in seconds with no VM, which matters because the touch behaviour itself cannot.
- [ ] **Every `TOOLS[]` name in `kdos-mobile/main.c` is symlinked by its `build.sh`**, and every
      symlink has a `TOOLS[]` entry. Both halves are one edit and each half alone is a program
      nothing can reach or a link error.
- [ ] **Every `# depends` in the two new recipes names a port that exists**; every recipe
      declares name, version, release and `sha256` where it has a source.
- [ ] **The mobile skel `rc.xml` keeps `<default />` as the first child of `<keyboard>` and of
      `<mouse>`** — the existing check, extended to the mobile file, with the comments stripped
      first so it does not report on the file's own explanation of the trap.
- [ ] **Nothing in the mobile chrome uses a hover-only affordance.** Crude on purpose, the way
      the flag check is: grep the two packages for the tooltip and hover entry points and require
      each call site to have a long-press or a visible label beside it.

**Checkpoint**
```sh
testing/preflight.sh 2>&1 | tail -30
```
Expected: every new check runs and passes; a deliberately broken flag, layout keysym, symlink or
`<default />` is caught and named.

### Task 9.2: Goldens and `testing/selftest.sh`

- [ ] `--dump` and `--dump-cells` for **every** mobile surface: `kdos-bar`, `kdos-nav`,
      `kdos-home` × 4 panes, `kdos-cards`, `kdos-shade`, `kdos-sheet`, `kdos-sel`, `kdos-osk` ×
      4 layouts.
- [ ] Two sizes each — **33x36 and 73x16** — because a geometry defect is usually a defect at ONE
      width, which is why the desktop's goldens are at 80x24 and 132x43.
- [ ] libkwl is stubbed out by `testing/fixtures/shell/dumpmain.c`'s shape so the dump path
      touches no Wayland and no fcft and runs on a host that has neither. libkicon is stubbed to
      answer -1, so **a golden is the CHARACTER grid** and a layout that only lines up once the
      pictures load is a layout that is broken.
- [ ] New library assertions in `src/libs/selftest.c`: `ktui_touch_min_rows/cols` across the cell
      ladder and against a zero-mm output; the cell-size ladder's fontconfig name rewriting
      (`Terminus:pixelsize=32` + 64 must yield `Terminus:pixelsize=64`, not both); the layout
      parser against a malformed row; the completion trie's ranking; and the gesture config
      parser against an unknown key.
- [ ] Run the suite sanitized whenever a parser is touched — `CC="cc -fsanitize=address,undefined -g"
      testing/selftest.sh` — the seam that already found two real defects in this tree.
- [ ] **Say what is not covered.** `selftest.sh` prints, beside the mobile goldens, that touch
      behaviour and gesture recognition are proven under `make test-mobile` and not here.

**Checkpoint**
```sh
testing/selftest.sh 2>&1 | grep -E 'mobile|touch|osk' | tail -20
```

### Task 9.3: Documentation

- [ ] `CLAUDE.md` gains sections mirroring the desktop's, in the same voice — **the current
      state and the constraint, never the changelog**: the mobile geometry table and its
      derivation; the deferred-press rule; the gesture and cancel rules; `kdos-osk`'s two
      protocol modes and why fcitx5 survives one of them; the per-output template sets; the
      window-fit mechanism; and the mobile CRT defaults.
- [ ] `docs/KDOS-MOBILE.md` NEW: the plan of record for this target, with a status block per
      wave, the way `docs/KDOS-DESKTOP.md` is described. **A doc that is stale-pessimistic costs
      a re-verification** — when a status paragraph and the tree disagree, measure the tree and
      fix the paragraph in the same pass.
- [ ] `docs/ACCESSIBILITY.md` gains the mobile statement of record: the measured touch-target
      sizes, the contrast of the mobile chrome against the same palette, the fact that pinch-to-
      zoom is the text-size mechanism and is therefore an accessibility feature rather than a
      novelty, and — stated plainly — that there is still no screen reader and there will not be
      one.
- [ ] `README.md` gains the mobile target and its screenshots.
- [ ] `kdos help` gains the gesture cheat sheet; `kdos-keys` gains a gestures page.

**Checkpoint**
```sh
grep -c 'mobile\|kdos-osk' CLAUDE.md
ls docs/KDOS-MOBILE.md
```

### Task 9.4: The device acceptance matrix — **MANUAL, needs the device**

The one table that says whether this worked. Each row is filled in by hand on real hardware, and
`skip` with a reason is a legitimate answer for anything the device genuinely cannot do — the
third report level `kdos doctor` already carries, and for the same reason: reporting `ok` for
something never tested is worse than reporting nothing.

- [ ] Fill in:

```
                                          fajita      qemu-aarch64
display, wallpaper, CRT pass              [   ]       [   ]
touch: tap / drag / long-press            [   ]       [   ]
kinetic scroll                            [   ]       [   ]
pinch steps the cell ladder               [   ]       [   ]
edge gestures + client cancel             [   ]       [   ]
gesture bound to a command                [   ]       [   ]
kdos-osk raises on a text field           [   ]       [   ]
Ctrl-C kills a process in foot            [   ]       [   ]
spacebar trackpad moves the caret         [   ]       [   ]
flick-up produces the shifted character   [   ]       [   ]
completion strip offers, never rewrites   [   ]       [   ]
haptics on key press                      [   ]       [ skip: no vibrator ]
home: four panes, search                  [   ]       [   ]
cards: ASCII thumbnails, flick to close   [   ]       [   ]
shade: notifications, quick settings      [   ]       [   ]
cell-granular selection and copy          [   ]       [   ]
PIN lock, notifications on the lock       [   ]       [   ]
rotation with hysteresis                  [   ]       [ skip: no accelerometer ]
split-screen tiling in landscape          [   ]       [   ]
external display + desktop chrome         [   ]       [   ]
external keyboard hides the OSK           [   ]       [   ]
oversized window fits and is reachable    [   ]       [   ]
Wi-Fi joins from quick settings           [   ]       [ skip: no wcn3990 ]
boxed app launches from the launcher      [   ]       [   ]
suspend and resume                        [   ]       [   ]
battery life, screen on, idle             [   ] h     [ skip ]
```

- [ ] Record the measured cold and warm appbox launch times, the measured idle battery life, and
      the measured GPU cost of the CRT pass with and without the battery floor. **Numbers
      measured on the phone, never the desktop's numbers reused.**

**Checkpoint** — the filled matrix, pasted into this task, with a reason beside every `skip` and
every empty box.

---

## Risk register

Carried openly rather than discovered. Each names what happens if it turns out badly, and none
of them blocks more than its own wave.

| # | Risk | Impact if it goes badly | Mitigation, decided now |
|---|---|---|---|
| R1 | **DP alt-mode absent on SDM845 mainline** | Task 6.4 cannot be demonstrated on hardware | Tasks 6.1–6.3 and 6.5 are proven on two QEMU outputs and need no phone. The wave lands; the row in the matrix reads `absent` with its evidence |
| R2 | **The QEMU rig's touch device is missing or its QMP multi-touch does not work** | Wave 2's gate has no mechanism | Task 1.3 verifies `virtio-multitouch-pci` in the built rig image **before** Wave 2 begins, and fails there rather than three waves later |
| R3 | **QEMU-only touch proof means gesture and tap-vs-drag regressions are caught per-wave, not per-change** | A regression can live for a whole wave | Accepted, and stated in `selftest.sh`'s own output. The cheap checks that CAN run per-change — the goldens and `--dump-layout --mm` — do |
| R4 | **The freedreno GLES2 stack rejects the CRT shader** | The phone does not look like KDOS | The pass already declines a renderer it cannot use and reports it at startup; `KDOS_CRT_DUMP` is how it is diagnosed without a screen. Worst case the phone runs with the pass off and the row says so |
| R5 | **fajita's touchscreen reports 0 mm** | Every millimetre in this plan falls back to constants | The fallback column exists for exactly this and is the second half of every geometry rule. Task 1.4 records the reported size either way |
| R6 | **Debian trixie lacks an arm64 build of a curated app** | The bake fails | Task 8.1 verifies each package for arm64 at bake time; the list is decided in the Containerfile, where a substitution is one line |
| R7 | **The repo grows by ~1.2 GB of LFS** | Clone cost | Accepted deliberately (decision 12) to keep KDOS property #3 intact for the mobile target. The curated image is a third of what the full set would have cost |
| R8 | **The moved application index changes `kdos-shell`'s behaviour** | The desktop regresses while the phone is being built | Task 5.1 lands first and its checkpoint is the desktop's own existing goldens, unchanged |
| R9 | **`wl_touch.cancel` in `input/touch.c` disturbs labwc's existing touch path** | Touch regresses for every client, not just KDOS chrome | The cancel is sent only on gesture COMMIT, which today never happens because no gesture is recognised. The path is inert until Wave 3 turns it on |
| R10 | **Two shells drift** | The phone and the desktop disagree about what an app is, what is pinned, what is boxed | Task 5.1 is the structural answer: one index and one favourites writer, in libraries, with `kdos-shell` and `kdos-mobile` both consumers |

---

## Self-Review

Checked against this plan's own rules before it was handed over.

**Placeholders.** No `TBD` and no `TODO`. Four things are deliberately left to be *measured* at
execution time rather than guessed here, and each says so at its use site with the source to read:
the fajita kernel config fragment for the panel and touchscreen (Task 1.4), the vibrator node
(Task 4.7), the wcn3990 firmware paths (Task 7.2), and the arm64 availability of each curated
Debian package (Task 8.1). Naming the source is the provenance rule; naming the value from
memory would violate it.

**Internal consistency.** The sixteen decisions in Global Constraints are each traceable to at
least one task. Decisions 11 and 12 appeared to conflict and are explicitly reconciled in Global
Constraints and again in Wave 8. The geometry table's numbers are used in Tasks 2.4, 4.2, 5.2 and
9.1 and nowhere contradicted; the landscape key-height relaxation appears once, with its
arithmetic. The exclusions list and the risk register do not overlap: nothing is both excluded and
mitigated.

**Scope.** Nine waves is large for one plan, which is a consequence of decision 4 (full
convergence) and decision 15 (Wi-Fi in). The waves are independently gated and Waves 7 and 8 could
be split into their own plans without disturbing Waves 1–6; that is stated here rather than left
to be discovered if the arc runs long.

**Ambiguity.** Three places where a reader could reasonably have gone two ways are pinned:
the press is deferred and the four outcomes are a table, not prose (Task 2.2); `keys` mode does
not bind the input-method manager at all, which is what leaves it for fcitx5 (Task 4.4); and the
completion strip becomes the hacker row from a **config list** of terminal app_ids, never a
hardcoded `foot` (Task 4.6).

**Rules kept.** No commit steps, per KDOS hard rule 5. Every task ends in a Checkpoint whose
output proves it landed. `kdos-shell` is modified nowhere; code moves out of it into libraries and
its own goldens are the proof. Every new config file is parsed and reports an unknown key by name.
No `system()` and no shell string in either new package. Every new source file carries the banner.

**What this plan does NOT prove.** Touch behaviour and gesture recognition are demonstrated under
QEMU only (decision 8) and cannot run in `testing/selftest.sh`; battery life, thermal behaviour,
suspend/resume and DP alt-mode can only be established on the device, which is what Task 9.4
exists for. A plan that claimed otherwise would be claiming something it had not earned.

# kdos-res

`kdos-res` is the KDOS resource monitor — the program the desktop calls **Resources**. It shows
what is using the machine across eleven pages: applications, processes, processor, memory,
graphics, drives, network, batteries, energy, sensors and boxes. It can also end, stop or renice a
process, through a small setuid helper called `kdos-resctl`.

This page is for anyone using it day to day, for administrators who want to know what the helper
may do, and for contributors changing it. If you only want to see what is using the machine, press
`Ctrl+Shift+Escape` and read [Pages](#pages) and [Keys](#keys); the rest of the page is reference.

It runs in three ways, and all three draw the same grid of character cells:

| Face | When |
|---|---|
| A window under the compositor | `$WAYLAND_DISPLAY` is set, or `--gui` |
| Full screen in the terminal it was started from | No Wayland display (over ssh, on `tty2`), or `--tty` |
| An offscreen text dump | `--dump`, for the test suite |

Which display server the window reaches is decided by
[libkdisp](../05-developer/c-libraries.md), the display library, not by `kdos-res` itself. There is
only ever one frame round the program. In the terminal and in a dump, `kdos-res` draws a box round
the whole grid with **Resources** on its top edge. In a window the compositor's own title bar and
border are that frame, so `kdos-res` draws none. If you ever see two frames nested one inside the
other, something is drawing chrome the compositor already drew, and that is a fault.

## Opening it

| From | How |
|---|---|
| The panel's meters strip | Left click |
| The keyboard | `Ctrl+Shift+Escape`, or `Super+Ctrl+T` |
| The Start menu | **Resources**, or **Boxes** (which opens the Boxes page) |
| A prompt | `kdos-res`, or `kdos-res --page boxes` |

The window asks the compositor to float, at 104x26 cells. A monitor is looked at and dismissed
rather than kept as one pane among others. The size is at or above the width where the sidebar
shows full names; it is a request, and on a screen too small for it the compositor's size wins.

## Synopsis

```
kdos-res [--page ID] [--tty | --gui] [--fixture DIR] [--interval MS]
         [--detail PID] [--font NAME]
         [--dump | --dump-cells] [--dump-size WxH] [--json]
         [--version] [--help]
```

## Options

| Option | Effect |
|---|---|
| `--page ID` | Open on a page. `ID` is one of the identifiers in [Pages](#pages); anything else exits 2 with the usage text and the page list |
| `--tty` | Draw in the terminal, whatever the environment says |
| `--gui` | Open a window, whatever the environment says. With no display server reachable it exits 1 and suggests `--tty` |
| `--fixture DIR` | Read a recorded system state instead of the live one. See [Fixtures and reference frames](#fixtures-and-reference-frames) |
| `--interval MS` | Sampling period in milliseconds, overriding the configuration. Floored at 200 |
| `--detail PID` | With `--dump`, draw the detail page for that process |
| `--font NAME` | The font for the window |
| `--dump` | Sample twice, render once offscreen and write the cells to standard output |
| `--dump-cells` | The same as `--dump` |
| `--dump-size WxH` | The offscreen grid. Default 80x24. A malformed value exits 2 |
| `--json` | With `--dump`, prepare the frame and write nothing |
| `--version` | Print `kdos-res <version>` and exit 0 |
| `--help`, `-h` | Print the usage text and exit 0 |

An unrecognised option exits 2 with a usage line rather than being ignored, so a program that
starts `kdos-res` with a flag it does not have fails visibly. The usage text reads its page list from
the page table itself, so it always names every page.

## What it shows that other monitors cannot

Every containerised application on KDOS runs in a box of its own, so an ordinary process list shows
rows of internal process names and no row for the application a person launched. `kdos-res` finds
each process's box by walking up its parent chain to the container monitor (`conmon`), whose
arguments name the box. The Processes page shows that name in a **BOX** column, the detail page
shows it on an `appbox` line, and the Applications page rolls a whole box up into one line.

![The Applications page: the page sidebar, the header band naming its subject, and a process identified as belonging to a box](../../screenshots/res-applications.png)

## Pages

Eleven pages, in sidebar order. The identifier is the one spelling: `--page` takes it, the
configuration's `sort` key names a column on it, and the committed reference frames are named after
it.

| Page | Identifier | Shows |
|---|---|---|
| Applications | `applications` | One row per application, however many processes it has. Sort by name, CPU, memory, disk or process count |
| Processes | `processes` | The process table: PID, user, CPU, memory, disk, box and name. Sort by CPU, memory, PID, name or disk; `/` filters by process name, command line, box or user (case-insensitive substring) |
| CPU | `cpu` | Per-core and aggregate processor time |
| Memory | `memory` | Memory and swap figures, with a note that per-slot memory module details are not shown |
| GPU | `gpu` | Graphics utilisation, or engine time where the driver publishes no percentage |
| Drives | `drives` | Block devices, capacity and throughput |
| Network | `network` | Interfaces and their rates |
| Batteries | `batteries` | Charge, rate and health |
| Energy | `energy` | Each application's share of energy use, asked of `kdos-energyd` |
| Sensors | `sensors` | Temperature, fan and voltage readings |
| Boxes | `boxes` | Each box: state, processes, CPU, memory, energy share, disk and uptime. Sort by name, CPU, memory, disk or process count |

![The Boxes page](../../screenshots/res-boxes.png)

**The Boxes page** is a rollup keyed on the same box lookup the process table uses. Its energy
column is the energy daemon's answer, asked for rather than recomputed, and it shows `-` when that
daemon is not running — never `0`, which would report a missing reading as an idle machine. A box
that has a profile but is not running is still listed, because it exists as far as the box manager
is concerned.

### The sidebar and its three widths

The sidebar is drawn from the same page table, and it has three widths, measured in cells:

| Window width | Sidebar |
|---|---|
| 100 cells or more | 18 cells, full page names |
| 60 to 99 cells | 6 cells, three-character prefixes |
| Under 60 cells | None; `F10` and `[` / `]` are the ways between pages |

The narrow form uses three characters rather than one initial, because a single letter would make
Batteries and Boxes the same control.

## Keys

`kdos-res` answers the keys every KDOS surface answers, described in
[the design language](../03-architecture/design-language.md#the-keys-every-surface-answers).

| Key | Does |
|---|---|
| `F1` | Opens this program's documentation page in `kdos-doc` |
| `F10` | The page list, as a dialog over the page; `F10` or `Enter` closes it |
| `[`, `]` | Previous and next page |
| `Tab` | Moves focus between the sidebar and the page |
| `↑`, `↓` | The sidebar's pages or the page's rows, depending on focus |
| `Enter` | Opens the detail page for the selected row |
| `Esc` | Unwinds one level. See below |
| `q` | Leaves |

Page keys:

| Page | Key | Does |
|---|---|---|
| Applications | `s` | Next sort column |
| Applications | `r` | Reverse the sort |
| Applications | `e` | End the selected application (every process in it), after a confirmation |
| Applications | `k` | Kill the selected application, after a confirmation |
| Processes | `s`, `r` | Next sort column; reverse the sort |
| Processes | `/` | Filter by process name, command line, box or user (case-insensitive substring). Type, `Backspace` to erase, `Enter` to keep the filter, `Esc` to clear it while you are still typing |
| Boxes | `s`, `S` | Next sort column; reverse the sort |
| Drives, Network | `↑`, `↓` | Select a row |
| Drives, Network | `Enter` | Open the facts page for the selected drive or interface |
| Network | `n` | Open the network settings (`kdos-net`) |
| Energy | `↑`, `↓` | Scroll |
| Energy | `g` | Ask `kdos-energyd` again |
| Detail | `←`, `→`, `Tab` | Move between the buttons |
| Detail | `↑`, `↓` | Scroll the facts |
| Detail | `Enter` | Press the focused button |
| A confirmation | `←`, `→`, `Tab` | Move between the two buttons |
| A confirmation | `y`, `Enter` on the confirm button | Confirm |
| A confirmation | `n`, `q`, `Esc` | Cancel |

`Esc` takes exactly one step per press:

| What is up | What `Esc` does | What the footer reads |
|---|---|---|
| A confirmation | Cancels it | `Esc Cancel` |
| A detail page | Back to the list it came from | `Esc Back` |
| The `F10` page list | Back to the page under it | `Esc Pages` |
| Nothing | Leaves the program, unless the page uses `Esc` itself (see below) | `Esc Close` |

The one page that uses `Esc` itself is Processes, and only while a filter is being typed: there
`Esc` clears the filter and stays on the page. Once `Enter` has kept a filter, the next `Esc`
leaves the program with the filter still set. To clear a kept filter, press `/` and then `Esc`.

While a confirmation is up it owns the keyboard, and neither `Esc` nor `q` leaves the program: a
question is waiting on its answer.

**The pointer.** Moving over a row highlights it, a click selects it, and a click on the row that is
already selected opens its detail page. The wheel scrolls the list while it is longer than the
window and moves the selection while it fits. The Applications, Processes and Boxes tables have a
scrollbar you can drag.

## The detail page

`Enter` on a process, an application, a box, a drive or a network interface opens a full-screen
page for that one subject. For a process or an application it shows identity, its own processor and
memory rings, thread count, open descriptors and elapsed time; for a box, a drive or an interface it
lists that subject's facts.

The rings start when the page opens. Keeping history for every process would be hundreds of rings,
and filling one with zeroes would invent a past the program did not watch.

The detail page carries five buttons: **End**, **Kill**, **Nice -**, **Nice +** and **Close**. The
first four work only on a process's page; on an application's page they are disabled, and the
Applications page's `e` and `k` act on the whole application instead. These buttons are the only
per-process verbs in the program: a key that ended a process straight from a table would act on
whatever row the cursor happened to be on, with nothing on the screen saying which. A button that
cannot work on a process is shown disabled with the reason on the same row — for example
"kdos-resctl has lost its setuid bit".

## Acting on a process

A process you own is signalled and reniced directly, with `kill(2)` and `setpriority(2)`. Anything
else, and any renice below zero, goes through `kdos-resctl`, the setuid helper. Its whole security
argument is that there is nothing to aim: three commands, no paths, no options.

```
kdos-resctl dmi                                   the SMBIOS table; its path is compiled in
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

| Exit status | Meaning |
|---|---|
| 0 | Done |
| 1 | Refused |
| 2 | Usage error |
| 3 | The operation itself failed |

The caller must be in the `wheel` group, by real user id — the same gate as `kdos-powerd` and
`kdos-energyd`. The group is fixed when the helper is built and cannot be changed by an environment
variable. The full argument is in [the security model](../03-architecture/security-model.md).

The helper is never on the sampling path: a setuid program started once a second would be an
attack surface on a schedule. `kdos doctor` checks that it exists and is setuid root, because losing
the bit is silent and the only symptom is a button that cannot work.

**Confirmations.** End and Kill are confirmed; the dialog is drawn by this program, Cancel is
preselected so a reflex `Enter` does not kill anything, and the message says what will happen. The
Applications page's `e` and `k` act on a whole application, and the count is why they are worth
confirming — here, an application is a container's worth of processes:

> End all Firefox — 41 processes in appbox app.firefox-esr. Unsaved work in them is lost.

A renice is not confirmed. It is reversible, and a dialog on every nudge teaches people to click
through the one that matters.

The desktop's own processes are confirmed like anything else rather than refused. `kdos-oomd`
protects the compositor, the panel, the desktop and the notification daemon because it acts on its
own initiative; a person ending a wedged panel is entitled to, and the supervisor restarts it.

## Reading the numbers honestly

No number is invented. Where the machine publishes no value, the reading is "unreadable" and the
cell shows `-`. A default of `0` is how a monitor reports a missing sensor as an idle machine.

The GPU page is where that matters most. Only some drivers publish a utilisation percentage; every
other driver gets engine time, labelled as such, and a driver with no statistics gets no column at
all rather than a column of zeroes.

Three further rules shape what you see:

- **A counter that went backwards is a gap, never a spike**, and both halves of a paired reading
  (received and sent) skip that sample together, so they stay in step.
- **Rates come from the sampler, not from drawing.** A frame is not an interval, and the dump draws
  once after two samples, so a rate computed while drawing would be empty in every reference frame.
- **Elapsed time is measured against system uptime.** A process's start time and the uptime are both
  seconds since boot; any other clock is a different epoch and, under a fixture, a different
  machine. A start later than the uptime shows `-`.

The sampling deadline is also the wait deadline. Keystrokes, pointer movement and resizes all wake
the loop, so a loop that always waited a full interval after each wake-up would sample unevenly and
draw an uneven chart. It waits only for whatever remains until the next sample.

## The charts

Charts are drawn as character cells. Whole rows are the full block, and the top row of each column
is the block-ramp character for the remainder, so the resolution is rows times the ramp's levels and
the shape survives all three glyph tiers of the
[design language](../03-architecture/design-language.md).

**For contributors.** A chart on these pages cannot be a pixel picture (a *tile*, described in
[Writing desktop software](../05-developer/writing-desktop-software.md#pixel-tiles)). The toolkit
stores a tile's position within a cell in four bits each way, so one tile covers at most 16x16
cells, and a page-wide chart is larger than that. Two toolkit behaviours matter when adding a
chart:

- The toolkit's one-row sparkline draws only in the first row of whatever band it is given. Given
  a ten-row band, it draws over the label in the first row and leaves the other nine empty.
- A tile's picture is held in a numbered *slot* that exists only once the tile has been drawn for
  the first time. Code that draws a tile only when its slot already exists therefore never draws
  it.

## Configuration

`~/.config/kdos/res.conf`, or `$XDG_CONFIG_HOME/kdos/res.conf` where that is set. It is plain
`key = value` lines with `#` comments, the same shape as `panel.conf`. A missing file means the
defaults. An unknown key is reported by name on standard error rather than ignored.

| Key | Values | Default | Does |
|---|---|---|---|
| `interval` | milliseconds | `1000` | Sampling period, clamped to 200–60000 |
| `units` | `1024`, `1000` | `1024` | Which power the size units use |
| `temperature` | `c`, `f` | `c` | Temperature scale |
| `cpu_percent` | `core`, `machine` | `core` | Whether a percentage is of one core or of the whole machine |
| `memory` | `rss`, `pss` | `rss` | Which memory figure a process row reports |
| `kernel_threads` | boolean | `no` | Show kernel threads in the process table |
| `virtual_drives` | boolean | `no` | Show loop and other virtual block devices |
| `virtual_net` | boolean | `no` | Show virtual network interfaces |
| `icons` | boolean | `yes` | Draw icons in the header band |
| `sort` | a column name | `cpu` | Initial sort column. Processes: `cpu`, `memory`, `pid`, `name`, `disk`. Applications and Boxes: `name`, `cpu`, `memory`, `disk`, `procs`. A name a page does not have leaves that page on its own default |
| `columns` | column list | empty | Read and stored; no page consults it |

`yes`, `1`, `true` and `on` are true for a boolean; anything else is false.

A short interval is floored rather than trusted: a monitor sampling every 10 ms becomes the load it
is measuring.

Example:

```
# ~/.config/kdos/res.conf
interval    = 2000
temperature = f
memory      = pss
sort        = memory
```

`SIGHUP` re-reads this file and the accent — the signal `kdos theme` sends to every running surface.
The handler only sets a flag, and the loop picks it up within one interval. The accent itself is the
one word in `~/.cache/kdos/theme` (`$XDG_CACHE_HOME/kdos/theme`).

## Fixtures and reference frames

```sh
kdos-res --fixture testing/fixtures/res --page boxes --dump
```

A **fixture** is a recorded system state (see the [glossary](../06-reference/glossary.md)).
`--fixture DIR` points every reader at `DIR/proc` and `DIR/sys` instead of the live ones, and uses
`DIR/passwd` for user names when it exists, so the same recording renders the same names on every
host. This is what makes the output deterministic enough to commit reference frames, called
**goldens**, at all.

A dump samples twice before drawing, against the fixture's two snapshots: `DIR/` and `DIR/next/`,
one interval apart. Every rate is a difference between two readings, so sampling one snapshot twice
would draw a machine doing nothing and a golden that could never catch an arithmetic error.

Goldens are committed in `testing/goldens/` for all eleven pages plus the detail page, at three
sizes — 36 files named `res-<page>-<size>.txt`:

| Size | Sidebar |
|---|---|
| 56x24 | None |
| 80x24 | Six cells of three-character prefixes |
| 132x43 | The full eighteen |

Under a fixture, the detail page does not ask whether `kdos-resctl` is installed: a fixture is a
recorded machine, the helper belongs to the running one, and the answer would make one fixture
render differently on two hosts. Nothing is executed against a fixture.

See [Testing](../05-developer/testing.md) for how the goldens are regenerated and compared.

## Files

| Path | What |
|---|---|
| `/usr/bin/kdos-res` | The monitor |
| `/usr/bin/kdos-resctl` | The helper, mode 4755 |
| `/usr/share/applications/kdos-res.desktop` | The **Resources** launcher |
| `~/.config/kdos/res.conf` | Configuration |

## Limitations

- The Drives and Network lists have no scrollbar. They are short, and their pointer handling is
  select-and-open with no viewport.
- The sampler records when one sample takes longer than the interval, but no page displays it.
- `columns` in the configuration is accepted and has no effect.
- Per-slot memory module details are not shown. `kdos-resctl dmi` reads the SMBIOS table, but no
  page asks it.
- `Esc` clears a Processes filter only while it is being typed. With a kept filter, `Esc` leaves
  the program; press `/` and then `Esc` to clear the filter instead.
- While a Processes filter is being typed, `q`, `[`, `]`, `Tab` and `F10` keep their global
  meaning, so a filter cannot contain those characters and typing `q` leaves the program.

## See also

- [The design language](../03-architecture/design-language.md) — the pointer contract and the key ladder
- [The daemons](daemons.md) — the energy daemon this asks, and the memory daemon it complements
- [The security model](../03-architecture/security-model.md) — the setuid helper in full
- [Testing](../05-developer/testing.md) — fixtures and reference frames
- [The kdos command](kdos-command.md) — the same readings from a prompt

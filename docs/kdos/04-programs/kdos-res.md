# kdos-res

`kdos-res` is the system monitor. It reports processor, memory, graphics, storage, network,
battery, energy, sensor and container readings across eleven pages, and it can end, stop or
renice a process through a small setuid helper.

It runs as a window under the compositor, as a full-screen program in a terminal, and as an
offscreen text dump for the test suite. All three draw the same grid of character cells.

## Synopsis

```
kdos-res [--page ID] [--tty | --gui] [--fixture DIR] [--interval MS]
         [--detail PID] [--font NAME]
         [--dump | --dump-cells] [--dump-size WxH] [--json]
         [--version] [--help]
```

## Description

Most process tables on this system answer the wrong question. Every containerised application here
is its own box, so an ordinary listing shows dozens of rows of internal process names and no row
for the application a person actually launched. `kdos-res` resolves a process id to its box through
the container supervisor's process chain, so a row reads `firefox-esr (appbox app.firefox-esr)`,
and the Applications page rolls those rows up into one line per application.

That identity is cheap here because the boundary already exists and the supervisor already knows
the name. It is the one reading a general-purpose monitor on this machine cannot produce.

![The Applications page: the page sidebar, the header band naming its subject, and a process identified as belonging to a box](../../screenshots/res-applications.png)

With neither `--tty` nor `--gui`, the program looks for `$WAYLAND_DISPLAY`. Where it is set, a
toplevel window opens; where it is not, the cells go to the terminal the command was typed in,
which is the right answer over ssh and on `tty2`. Which display server the window reaches is
[libkdisp](../05-developer/c-libraries.md)'s decision — `kdos-res` registers one implementation and
calls `kdisp_init`.

The window asks for 104x26 cells. That is at or above the width where the sidebar degrades, and
the generic toplevel default is below it, so a window naming no size opens permanently in the
narrow band with the sidebar collapsed and the footer hint clipped mid-word. The request is a
default rather than a demand: the compositor's first configure wins on a screen too small for it.

One frame is drawn, never two. Undecorated — in a terminal, in a dump — the program draws a box
around the whole surface with its title on the top edge. Under the compositor the server-side
decoration is that box, so the drawn one is suppressed and only its inset is kept. Two boxes
nested one inside the other is the tell that a program drew chrome the compositor had already
drawn.

## Options

| Option | Effect |
|---|---|
| `--page ID` | Open on a page. `ID` is one of the identifiers in [Pages](#pages); anything else exits 2 with the list |
| `--tty` | Draw in the terminal, whatever the environment says |
| `--gui` | Open a window, whatever the environment says |
| `--fixture DIR` | Read a recorded system state instead of the live one. See [Fixtures](#fixtures-and-reference-frames) |
| `--interval MS` | Sampling period. Floored at 200 ms |
| `--detail PID` | Open the detail page for one process. Dump only |
| `--font NAME` | The font for the windowed face |
| `--dump` | Render once offscreen and write the cells to standard output |
| `--dump-cells` | The same as `--dump` |
| `--dump-size WxH` | The offscreen grid. Default 80x24 |
| `--json` | With `--dump`, prepares the frame and writes nothing |
| `--version`, `--help` | Print and exit |

An unrecognised option exits 2 with a usage line rather than being ignored, so a program that
spawns `kdos-res` with a flag it does not have fails visibly.

The help text reads its page list out of the page registry rather than repeating it. One
hand-maintained copy of that list is one list that can fall short of the table while the flag it
documents keeps working, which nobody can detect without already knowing the missing name.

## Pages

Eleven pages, registered in one table in sidebar order. The identifiers in that table are the only
spelling: `--page` takes them, the configuration's sort key uses them, and the committed reference
frames are named after them.

| Page | Shows |
|---|---|
| `applications` | One row per application, however many processes it is |
| `processes` | The process table |
| `cpu` | Per-core and aggregate processor time |
| `memory` | Memory and swap |
| `gpu` | Graphics utilisation or engine time |
| `drives` | Block devices, capacity and throughput |
| `network` | Interfaces and their rates |
| `batteries` | Charge, rate and health |
| `energy` | The per-application energy share, asked of `kdos-energyd` |
| `sensors` | Temperature, fan and voltage readings |
| `boxes` | Box, state, processes, CPU, memory, energy share, disk, uptime |

![The Boxes page](../../screenshots/res-boxes.png)

The Boxes page needs no subsystem of its own. The container-supervisor walk already turns a process
id into a box name, and the page is a rollup keyed on that. Its energy column is the energy
daemon's answer, asked for rather than recomputed, and it renders as a dash when that daemon is not
running — never a zero, which is how a monitor reports a missing sensor as an idle machine. A box
that is described and not running is still a row, for the same reason the box manager reads the
profiles as well as the container engine.

### The sidebar and its three widths

The sidebar is drawn from the same page registry the help text reads, and it has three states,
measured in cells because that is the unit the whole program works in:

| Window width | Sidebar |
|---|---|
| 100 cells or more | 18 cells, full page names |
| 60 to 99 cells | 6 cells, three-character prefixes |
| Under 60 cells | None; `F10` is the only way between pages |

The fallback is three characters rather than one, and the toolkit's tab strip takes the prefix
where a name does not fit. A single initial makes Batteries and Boxes the same control, which is
worse than a truncation: a truncation at least reads as incomplete.

`F10` opens the page list as a modal over the body. The list is this program's menu, so it is on
the menu key; `F1` opens this program's page under `/usr/share/kdos/doc`, which is what `F1` means
on every surface here. `[` and `]` step between pages without opening anything.

## Keys

`kdos-res` answers the contract every surface answers, described in
[the design language](../03-architecture/design-language.md#the-keys-every-surface-answers).

| Key | Does |
|---|---|
| `F1` | This page, in the documentation viewer |
| `F10` | The page list, over the body |
| `[`, `]` | Previous and next page |
| `Tab` | Move focus between the sidebar and the page |
| `↑`, `↓` | The sidebar's pages, or the page's rows, depending on focus |
| `Enter` | Open the detail page for the selected row |
| `Esc` | Unwind one level. See below |
| `q` | Leave |

Three raised states are declared rather than written into an `Escape` arm, and `Escape` takes
exactly one of them per press:

| What is up | What `Escape` does | What the footer reads |
|---|---|---|
| A question about ending a process | Answers it, rather than dismissing it | `Esc Cancel` |
| A detail page | Back to the list it came from | `Esc Back` |
| The `F10` page list | Back to the page under it | `Esc Pages` |
| Nothing | The page's own back-out, and only then the program | `Esc Close` |

While a confirmation is up it owns the keyboard, and neither `Esc` nor `q` leaves the program:
something is waiting on that answer, and a dialog dismissed without one leaves it waiting.

## The detail page

`Enter` on a process, an application, a drive or an interface opens a full-screen page for that one
subject: identity, its own processor and memory rings, thread count, open descriptors and elapsed
time.

The rings start at the moment the page is opened. Keeping a ring per process would be hundreds of
them, and back-filling one with zeroes would be inventing a past the program did not watch.

End, Kill and Nice live on this page and nowhere else. A key that ended a process from a table
would be a key pressed while the cursor happens to be on a row, with nothing on the screen saying
which row that is.

## Acting on a process

`kdos-resctl` is the setuid helper. Its entire security argument is that there is nothing to aim:
three verbs, no paths, no options.

```
kdos-resctl dmi
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

The full argument is in [the security model](../03-architecture/security-model.md). Two properties
belong here. The helper is never on the sampling path, because a setuid fork once a second is an
attack surface with a schedule. And `kdos doctor` checks its setuid bit for the same reason it
checks the password checker's: losing it is silent, and the symptom is a verb that reports failure
with no explanation.

One confirmation dialog stands in front of the destructive verbs, and it names its subject. The
toolkit's own modal belongs to a frame protocol this program does not drive, so the dialog is this
program's. Cancel is preselected — a destructive button under the caret turns a reflex `Enter` into
a kill — and the message says what will happen:

> End all Firefox — 41 processes in appbox app.firefox-esr. Unsaved work in them is lost.

That count is why the Applications page's verbs are worth confirming at all: here, an application
is a container's worth of processes.

A renice is not confirmed. It is reversible, and a dialog on every nudge teaches people to click
through the one that matters.

The desktop's own chrome is confirmed rather than refused. `kdos-oomd` protects the compositor, the
panel, the desktop and the notification daemon because it acts on its own initiative; a person
aiming at a wedged panel is entitled to end it, and the supervisor brings it back. The dialog
names what will happen instead of declining.

## Reading the numbers honestly

No number is invented. Every reader answers "unreadable" where the machine publishes no value, and
the cell renders a plain `-`. A `0` default is how a monitor reports a missing sensor as an idle
machine.

The GPU page is where that bites hardest. Only some drivers publish a utilisation percentage, so
every other driver gets engine time, labelled as such, and a driver with no statistics at all gets
no column rather than a column of zeroes.

Three further rules shape what is displayed:

- A counter that went backwards is a gap, never a spike, and both halves of a mirrored pair skip it
  together. One half advancing while the other did not would put received and sent a sample out of
  step for the rest of the session.
- A rate is fed from the sampler, never from a page's per-frame preparation. Preparation runs once
  per frame and a frame is not an interval; the offscreen dump draws exactly once after two
  samples, so a chart fed from preparation is empty in every reference frame and the arithmetic
  behind it is checked by nothing.
- Elapsed time is computed against system uptime and against nothing else. A process's start time
  and the uptime are both seconds since boot; pairing the start time with the sampler's monotonic
  stamp is a different epoch, and under a fixture a different machine — the subtraction underflows
  and draws the first digits of an enormous number. A start later than the uptime renders a dash.

The sampling loop's deadline is also its poll deadline. Events wake this loop — a keystroke, a
pointer crossing a row, a configure — so a poll that always waits the full interval samples at
irregular intervals, and a chart then plots one uneven sample per pixel. The wait is whatever
remains until the next sample and never more. A tick that overruns its own interval is reported on
the screen: a monitor that has become the load it is measuring should say so rather than quietly
becoming the machine's top consumer.

## The charts

A full-width chart cannot be a sprite tile. The toolkit encodes a tile's sub-cell coordinate in
four bits each way, so one sprite slot covers at most 16x16 cells — the size the panel's meters
strip is built around. A page-wide chart here is many times that, so the pixel path is not merely
unused, it is unreachable.

The charts are therefore drawn as cells. Whole rows are the full block and the top row of each
column is the ramp character for the remainder, so the resolution is rows times the ramp's level
count and the shape survives all three glyph tiers.

Two shapes to avoid when adding one. The toolkit's one-row sparkline handed a ten-row band draws in
the first row, on top of the label, leaving nine empty — which on a real screen reads as a chart
that is not working. And a tile guarded by a check for an existing slot can never be created,
because the slot only exists after the commit that guard prevents.

## Configuration

`~/.config/kdos/res.conf`, or `$XDG_CONFIG_HOME/kdos/res.conf` where that is set. Flat
`key = value`, the same shape `panel.conf` uses. An unknown key is reported by name on standard
error rather than ignored: a line that does not take effect and says nothing is indistinguishable
from a setting that does nothing.

| Key | Values | Default | Does |
|---|---|---|---|
| `interval` | milliseconds | `1000` | Sampling period, clamped to 200–60000 |
| `units` | `1024`, `1000` | `1024` | Which power the size units are |
| `temperature` | `c`, `f` | `c` | Temperature scale |
| `cpu_percent` | `core`, `machine` | `core` | Whether a percentage is of one core or of the machine |
| `memory` | `rss`, `pss` | `rss` | Which memory figure a process row reports |
| `kernel_threads` | `yes`, `no` | `no` | Show kernel threads in the process table |
| `virtual_drives` | `yes`, `no` | `no` | Show loop and other virtual block devices |
| `virtual_net` | `yes`, `no` | `no` | Show virtual network interfaces |
| `icons` | `yes`, `no` | `yes` | Draw glyphs beside rows |
| `sort` | a page identifier's column | `cpu` | Initial sort key |
| `columns` | column list | empty | Column selection |

`yes`, `1`, `true` and `on` are all accepted for a boolean.

The interval is floored rather than trusted. A monitor asked to sample every 10 ms becomes the load
it is measuring, and the rates it prints are then mostly its own.

`SIGHUP` re-reads this file and the accent, on the same signal `kdos theme` already sends. The
handler sets a flag rather than doing the work: reparsing a file inside a signal handler means
allocating inside one, and the loop is never more than one interval away from noticing. A program
on the session's retint list that does not handle `SIGHUP` is killed by `kdos theme amber` and
comes back looking retinted, which is the same picture with a different process id.

## Fixtures and reference frames

```sh
kdos-res --fixture testing/fixtures/res --page boxes --dump
```

`--fixture` points every reader at a recorded system state instead of the live one — the same seam
the stutter attribution, the memory daemon and the privacy indicator use. It is what makes a
monitor's output deterministic enough to have committed reference frames at all. The fixture's own
`passwd` file is used too, because resolving a uid against the developer's `/etc/passwd` makes a
recorded machine render a different name on every host.

A dump samples twice before drawing, against the fixture's two snapshots — `<fixture>/` and
`<fixture>/next/`, one interval apart. Every rate on every page is a difference between two
readings, so sampling one snapshot twice renders a machine doing nothing at all, and a reference
frame of that can never catch an arithmetic error.

Frames are committed for all eleven pages plus the detail page, at three widths: 56x24, where the
sidebar is gone entirely; 80x24, where it is six cells of three-character prefixes; and 132x43,
where it is the full eighteen.

A reference frame that reads the host's filesystem is not a reference frame. The detail page's
footer explains why a verb is unavailable, which means stat-ing a helper binary — and the same
fixture on two machines then produces two different frames. Under a fixture that question is not
asked: a fixture is a recorded machine and the helper is a property of the running one, and
nothing is executed against a fixture in any case.

## Opening it

| From | How |
|---|---|
| The panel's meters strip | Left click |
| A keyboard | `Ctrl+Shift+Escape`, or `Super+Ctrl+T` |
| The Start menu | The Resources entry |
| A prompt | `kdos-res` |

The window asks the compositor to float. A monitor is looked at and dismissed rather than kept as
one pane among others, which is the same request `btop`'s entry makes with `X-KDOS-Float`. It is
set in the program rather than in a desktop entry because this surface attaches for itself, and a
key on a `Terminal=false` row is a key nothing reads.

## Limitations

The Drives and Network lists do not scroll. They are short, and their pointer handling is
select-and-open with no viewport, so the scrollbar and its drag exist on the two long tables only.

## See also

- [The design language](../03-architecture/design-language.md) — the pointer contract and the key ladder
- [The daemons](daemons.md) — the energy daemon this asks, and the memory daemon it complements
- [The security model](../03-architecture/security-model.md) — the setuid helper in full
- [Testing](../05-developer/testing.md) — fixtures and reference frames
- [The kdos command](kdos-command.md) — the same readings from a prompt

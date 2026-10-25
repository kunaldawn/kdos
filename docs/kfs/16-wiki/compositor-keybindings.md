# Compositor keybindings

Every desktop-wide key chord and pointer binding on KDOS belongs to the [compositor]{.idx}, `kdos-comp`,
and is read from one file, `~/.config/kdos-comp/rc.xml`. This page covers that file as a task:
finding which chords exist, adding, changing and removing a [key binding]{.idx}, the pointer and
touchpad settings, [workspaces]{.idx term="workspace"}, per-window rules, applying a change without logging out, and what to
check when a chord does nothing. The file is in labwc's format, because the compositor is a fork of
labwc; how the fork reads it and what it adds is in
[kdos-comp](../../kdos/04-programs/kdos-comp.md#bindings), and the full list of shipped chords is
in [Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts).

## Installation

[kdos-comp](#port-kdos-comp) reads the bindings; it is built in the `50_desktop` phase and is on
every image. There is nothing to enable.

The file a new account gets comes from the base filesystem, `fs/etc/skel/.config/kdos-comp/rc.xml`,
copied into the home directory when the account is created. The compositor also searches
[`/etc/xdg/kdos-comp/`]{.idx term="/etc/xdg/kdos-comp"}, where nothing is shipped. Editing the copy in `/etc/skel` changes the
default for accounts created afterwards and leaves existing ones alone.

Programs the bindings start come from their own packages: the shell surfaces from
[kdos-shell](#port-kdos-shell), the terminal from [foot](#port-foot), the power verbs from
[kdos-powerd](#port-kdos-powerd), the `kdos` command from [kdos-tools](#port-kdos-tools). The
shortcut card on `Super+F1` is `kdos-keys`, from kdos-shell, and is generated from your own
[`rc.xml`]{.idx} each time it opens.

The live medium and an installed system ship the same file. On the live medium the home directory
is on the boot overlay, so an edit lasts until power-off.

## Configuration

### The rule at the top of the file

The shipped file opens with the one rule that matters more than any binding:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=7-21 pin=fdcaacdb35cf lang=xml -->

### The keyboard section

The first bindings, after `<default />`. `W` is Super, `S` Shift, `C` Ctrl and `A` Alt; the key
after the last hyphen is an XKB key name:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=110-129 pin=f0f29bada4b6 lang=xml -->

`Super+Escape` shows the shape of a binding with several actions: it writes the session list,
then asks through `kdos-prompt`, then exits. A [run-or-raise]{.idx} key looks for a window by its
application identifier and either raises it or starts the program:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=178-196 pin=b92833b13631 lang=xml -->

### Workspaces

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=101-108 pin=3796b507ad9d lang=xml -->

The keys for workspaces 5 to 9 are already bound, so raising `number` is the only edit needed for
more.

### Pointing devices

The `<libinput>` block ships as a comment, as a template; the compositor's own defaults apply,
with tap-to-click on:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=497-518 pin=8fd2d5fcc198 lang=xml -->

### The mouse section

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=520-557 pin=3840975a8bc3 lang=xml -->

### Window rules

Three worked examples ship commented out:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=559-587 pin=adbfda59372d lang=xml -->

### What does not belong here

`~/.config/kdos/comp.conf` holds the compositor's KDOS settings (wallpaper, the phosphor pass,
idle and lid, the panel's shape, the keyboard aids) and refuses a line beginning `bind`, `startup`,
`workspaces` or `mouse`: the next reload logs the file and line number, says that the key
belongs in `~/.config/kdos-comp/rc.xml`, and ignores it. The keyboard layout comes from the
environment, not from `rc.xml`; see [Session and environment](session-and-environment.md#changing-the-keyboard-layout-for-the-session).

## Usage

### Finding the chords that exist

1. Press `Super+F1`. The card reads `~/.config/kdos-comp/rc.xml` itself each time it opens, so it
   follows your edits before any reload, and it shows a binding the compositor refused as well as
   one it took; a file it cannot read at all puts `rc.xml unreadable - built-in defaults shown` on
   the card. A run-or-raise key whose program is not installed is left off the card.
   `kdos-keys --print` writes the same card to the terminal.
2. Press `Super+Space` and type a word such as `tile` or `workspace`. The palette's chord rows show
   the chord for a matching action; Enter on one runs its program where it has one, and otherwise
   the row names the keys to press.
3. To see what a key is called, run `wev` in a terminal, focus its window and press the key; the
   name it prints after `sym:` is the name `rc.xml` takes.

### Adding a binding

1. Open the file:

   ```bash
   $EDITOR ~/.config/kdos-comp/rc.xml
   ```

2. Inside `<keyboard>`, below `<default />` and below the shipped bindings, add the binding. To
   make `Super+B` open Firefox:

   ```xml
   <keybind key="W-b"><action name="Execute" command="firefox-esr"/></keybind>
   ```

   `Execute` splits its command into words, honouring quotes, and runs it without a shell, so
   variables, globs and pipes are passed through as literal text; put anything that needs a shell
   in a script and name the script. See
   [The prompt command](../../kdos/04-programs/kdos-comp.md#the-prompt-command).

3. Check that the file is still well-formed XML:

   ```bash
   xmllint --noout ~/.config/kdos-comp/rc.xml
   ```

   No output means it parses.

4. Reload. Either of these does it:
   - `kdos-comp -r` in a terminal the compositor started;
   - `pkill -HUP -x kdos-comp` from any shell of yours, including `tty2` and SSH.

5. Press the chord.

### Changing a shipped binding

Add a binding for the same chord after the shipped one. Of two bindings with the same chord, the
later one wins, so the shipped line can stay where it is. For example, to make `Super+Ctrl+V`
open the clipboard history picker:

```xml
<keybind key="W-C-v"><action name="Execute" command="kdos-clip --pick"/></keybind>
```

### Removing a binding

Bind the chord to the `None` action after the binding it should cancel. A binding left with no
action is dropped, so the chord reaches the focused application instead:

```xml
<keybind key="W-q"><action name="None"/></keybind>
```

Deleting the shipped line also removes the binding, except for a chord that labwc binds itself
through `<default />` (`Super+Return`, `Super+A`, `Super+D`, the `Super`+arrow snaps, `Alt+Space`
and the volume and brightness keys): there labwc's own binding comes back. `None` works for every
chord.

### Binding a KDOS action

The compositor adds actions of its own to labwc's. The three window-group actions are not bound in
the shipped file, and `Super+g`, `Super+Ctrl+g` and `Super+Alt+g` are free:

```xml
<keybind key="W-g"><action name="AddToTabGroup"/></keybind>
<keybind key="W-C-g"><action name="RemoveFromTabGroup"/></keybind>
<keybind key="W-A-g"><action name="NextInTabGroup"/></keybind>
```

The six accessibility switches are bound to `Super+Alt+S`, `L`, `B`, `D`, `C` and `K`; see
[KDOS actions](../../kdos/04-programs/kdos-comp.md#kdos-actions).

### Running an action from a script

Any labwc action that takes no argument can be run on a window without a key:

```bash
kdos hey list                  # the window ids
kdos hey run ToggleMaximize 42
```

The state column of `kdos hey list` then shows `M`. An id that names no window prints
`kdos hey: no view with that id` and exits 1.

See [`kdos hey`](../../kdos/04-programs/kdos-command.md#kdos-hey).

### Setting the touchpad

1. Uncomment the `<libinput>` block, or copy the settings you want out of it, keeping it outside
   `<keyboard>` and `<mouse>`.
2. Set the values, for example `<naturalScroll>yes</naturalScroll>`.
3. Reload as in step 4 of [Adding a binding](#adding-a-binding).

### Adding workspaces

1. Change `<desktops number="4">` to the number you want, up to 9 for the shipped keys, and add a
   `<name>` for each new one if you want names.
2. Reload. How the panel's strip shows the extra workspaces is in
   [Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts).

### Sending an application to a workspace

1. Copy the second example out of the window-rules comment into a `<windowRules>` element of its
   own, after `</mouse>`.
2. Change `sandboxAppId` to the box the application runs in (`kdos hey list` prints a window's box)
   or use `identifier="<app_id>"` for a host program, and `to=` to the workspace number.
3. Reload. A rule applies when a window first appears, so close the application and open it again.

### Going back to the shipped file

```bash
cp ~/.config/kdos-comp/rc.xml ~/.config/kdos-comp/rc.xml.mine
cp /etc/skel/.config/kdos-comp/rc.xml ~/.config/kdos-comp/rc.xml
pkill -HUP -x kdos-comp
```

## Tips

- Keep your own bindings together at the end of `<keyboard>`, below a comment of your own. When
  the shipped file changes, copying it over and pasting your block back is one step.
- A comment must not contain `--`; write `app-id` rather than `--app-id` in a comment, or the file
  stops being XML.
- Key repeat is set in `<keyboard>`, with `<repeatRate>` (repeats per second) and `<repeatDelay>`
  (milliseconds before the first repeat).
- `Alt+Tab`, `Alt+Shift+Tab` and `Alt+F4` are not in the file: they come from the compositor's
  defaults through `<default />`.
- The window menu (`Alt+Space`, or a right press on a title bar) and the compositor's root menu
  are defined in `~/.config/kdos-comp/menu.xml`, in labwc's [`menu.xml`]{.idx} format. A right click on the
  wallpaper opens the root menu only with `desktop_icons = no` in `comp.conf`; with desktop icons
  on, it opens the desktop's own menu, which `kdos-shell` draws and `menu.xml` does not define.
- labwc's manual pages are not installed. `labwc-config(5)` and `labwc-actions(5)` are at
  [labwc.github.io](https://labwc.github.io/); the labwc release the compositor was forked from is
  named in `src/desktop/kdos-comp/KDOS-FORK`.

## Troubleshooting

**The mouse does nothing in windows: clicks do not focus, title bars do not drag.** `<mouse>` has
lost its `<default />` line. labwc loads its built-in mouse bindings only through that line, or when
the file binds no button at all, and the shipped `<mouse>` binds some. Put `<default />` back as
the first child of `<mouse>` and reload. The same loss in `<keyboard>` takes `Alt+Tab` and
`Alt+F4` with it. A binding placed before `<default />` is not lost, but labwc's binding for the
same chord replaces it.

**Every chord of yours is missing, with no message on screen, and only labwc's own (`Alt+Tab`,
`Super+Return`) work.** The file does not parse, so the compositor binds nothing from it and falls
back to labwc's built-in bindings. Run `xmllint --noout ~/.config/kdos-comp/rc.xml`, which names the line, and look in the
compositor's log:

```bash
grep -E 'parser error|error parsing config file|unknown keybind|Invalid keybind|Invalid action' "$XDG_RUNTIME_DIR/kdos-comp.log"
```

A file that does not parse logs libxml2's message, such as
`Entity: line 495: parser error : Double hyphen within comment`, and then
`error parsing config file`. `unknown keybind (NoSuchKey)` followed by
`Invalid keybind: W-NoSuchKey` names a key the compositor could not resolve, and that one binding
is skipped; `Invalid action: NoSuchAction` names a misspelt action. The `Super+F1` card reads the
file on its own and is no guide to what the compositor loaded.

**`kdos-comp -r` prints `LABWC_PID not set` and exits 1.** The shell was not started by the
compositor. Use `pkill -HUP -x kdos-comp` instead.

**A run-or-raise key opens a second window every time.** The key finds the window by its
application identifier, and a terminal started as `foot -e <program>` carries `foot`'s own. A key
of your own must start the program as `foot --app-id=<program> -e <program>`, with the same name in
its `<query identifier>`, as the shipped keys do.

**A chord shows nothing at all.** Two shipped chords are known to do that; see
[Known gaps](#known-gaps).

**A keyboard aid is on and you do not know why.** Each switch posted a notification when it
changed; `Super+Shift+N` opens the notification centre. Press the same chord again to turn it off.

## Known gaps

- [Two shipped chords and two routes show nothing](../../kdos/06-reference/known-gaps.md#two-shipped-chords-and-two-routes-show-nothing)
- [The control centre has no pointer page](../../kdos/06-reference/known-gaps.md#the-control-centre-has-no-pointer-page)
- [Nothing starts the screensaver on idle](../../kdos/06-reference/known-gaps.md#nothing-starts-the-screensaver-on-idle)
- [A touchpad scroll is not one-to-one with the finger](../../kdos/06-reference/known-gaps.md#a-touchpad-scroll-is-not-one-to-one-with-the-finger)

## See also

- [Bindings](../../kdos/04-programs/kdos-comp.md#bindings): the two rules that break the whole file, and the KDOS actions
- [Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts): every chord the image ships
- [`~/.config/kdos-comp/rc.xml`](../../kdos/06-reference/configuration.md#configkdos-comprcxml): the file in the configuration reference
- [The window model](../../kdos/03-architecture/window-model.md): what snapping, tiling and placement do
- [Session and environment](session-and-environment.md), [The panel and the launcher](panel-and-launcher.md),
  [Accessibility](accessibility.md), [Screen lock](screen-lock.md), [Desktop troubleshooting](desktop-troubleshooting.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 412.* Previous: [411. Session and environment](session-and-environment.md) · [Contents](../README.md) · Next: [413. The panel and the launcher](panel-and-launcher.md)

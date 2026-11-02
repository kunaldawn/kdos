# Desktop troubleshooting

This page is for a desktop that does not come up, comes up wrong, or stops: what each stage from
the end of the boot to a working Plasma session leaves behind, where its log is, and how to get a
working desktop back from the recovery console. The stages themselves are described in
[The session](../../kdos/03-architecture/session.md); the settings each problem leads to are on the
other pages of this part.

## Installation

Nothing is installed for this page. The tools it uses are on every image:

| Tool | From | Used for |
|---|---|---|
| `kdos doctor` | [kdos-tools](#port-kdos-tools) | One report on the whole machine; its Session section checks the session bus, elogind and, inside a session, KWin, plasmashell and the KDE portal; its Desktop section the lock screen's PAM stack and setuid helpers, Xwayland and fcitx5 |
| `loginctl` | [elogind](#port-elogind) | Which sessions exist, which is active, and unlocking or ending one |
| `qdbus6` | [qt6-qttools](#port-qt6-qttools) | Asking KWin, plasmashell and the other session programs over the bus |
| `service --user` | [kdos-tools](#port-kdos-tools) | The user's own services: the session bus, PipeWire, the timers |

## Configuration

### Where each stage logs

| Stage | Log |
|---|---|
| `kdos-dm`, which starts SDDM or falls back to a text prompt | syslog, `grep kdos-dm /var/log/messages` |
| SDDM and the greeter, the greeter's KWin included | `/var/log/sddm.log`, the previous boot's in `/var/log/sddm.log.old` |
| The session wrapper, startplasma, KWin, plasmashell and everything they print | `~/.local/share/sddm/wayland-session.log` |
| The user's services, and every program the session bus starts | `$XDG_RUNTIME_DIR/ksvc/kdos-svc.<name>.log` |
| elogind | `/run/kdos-svc.elogind.log` and syslog |
| A crash of KWin or of an application | The session log; the crash handler DrKonqi is not installed |

### The recovery console

`tty2` is a plain login prompt that nothing on `tty1` can take away; see
[Login and autologin](login-and-autologin.md#logging-in-on-the-recovery-console). Every procedure
below starts there: `Ctrl+Alt+F2` from the desktop or the greeter, or `Alt+F2` at a text console.

## Usage

### Finding which stage stopped

1. Log in on `tty2` as the user whose desktop failed.
2. Ask the machine:

   ```bash
   kdos doctor
   loginctl list-sessions        # a session of `sddm` is the greeter; one of yours is the desktop
   service status sddm 2>/dev/null; pgrep -a sddm
   ```

3. Read the log of the last stage that ran, from the table above.

### Starting the greeter again

```bash
sudo pkill -x sddm
```

`init` runs `kdos-dm` again, which starts SDDM. Three exits within a minute put a text prompt on
`tty1` instead; it tries SDDM again once the minute is past.

### Starting the desktop by hand from tty2

To see what a session prints, start one on `tty2` itself:

```bash
startplasma-wayland 2>&1 | tee ~/plasma.log
```

It runs as the `tty2` session; leave it with Log Out.

### Resetting the desktop's configuration

A setting that keeps the session from starting is in `~/.config`. Move the suspects aside from
`tty2`, then log in again:

```bash
mkdir -p ~/config-aside
mv ~/.config/plasma-org.kde.plasma.desktop-appletsrc ~/.config/plasmashellrc ~/config-aside/   # the panels
mv ~/.config/kwinrc ~/.config/kwinoutputconfig.json ~/config-aside/                            # KWin and the screens
```

The KDOS defaults in `/usr/share/kdos/xdg` take their place. Moving the whole of `~/.config` aside
starts the account as new.

## Tips

- A virtual machine with a plain virtual display runs KWin on software rendering: it works, more
  slowly, and Phosphor mode is not available. `qdbus6 org.kde.KWin /KWin supportInformation |
  grep -i renderer` says which renderer is in use.
- A desktop that freezes while the pointer still moves is plasmashell, not KWin:
  `plasmashell --replace &` from KRunner (`Alt+Space`) or Konsole.
- A desktop that freezes with the pointer is KWin; switch to `tty2` and back, which makes KWin
  take the display again, before anything else.

## Troubleshooting

**A black screen after the splash, and `tty2` answers.** The greeter is not drawing. Read
`/var/log/sddm.log`. A line about the display (no DRM device, a permission refused) means KWin
could not take the screen: `ls -l /dev/dri/` and [Graphics drivers](graphics-drivers.md). A
greeter that SDDM keeps restarting shows the restarts there.

**`tty1` shows a text password prompt.** `kdos-dm` gave up on SDDM; see
[Login and autologin](login-and-autologin.md#troubleshooting).

**The greeter comes back after every login.** The session ended at once. The last lines of
`~/.local/share/sddm/wayland-session.log` name why; a refusal from `kdos-session-wrapper` names the
session bus or the runtime directory. See [Session and environment](session-and-environment.md#troubleshooting).

**The session starts with no panel.** plasmashell failed: `plasmashell --replace &` and read what
it prints. A broken layout is fixed with `kdos desk kdos`.

**Everything is very slow and Phosphor mode refuses.** KWin is on software rendering. Check the
renderer as in [Tips](#tips) and see [Graphics drivers](graphics-drivers.md).

**Applications start without the KDOS look, or notifications never appear.** The session runs
with the wrong environment or bus; see
[Session and environment](session-and-environment.md#troubleshooting).

**The screen is locked and the password is refused.** See
[Screen lock](screen-lock.md#troubleshooting).

**No sound in the session.** See [Audio](audio.md).

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [The session](../../kdos/03-architecture/session.md): every stage from the greeter to the desktop
- [Login and autologin](login-and-autologin.md), [Session and environment](session-and-environment.md),
  [Screen lock](screen-lock.md), [Graphics drivers](graphics-drivers.md),
  [Diagnostics](diagnostics.md), [Recovering an unbootable system](recovering-an-unbootable-system.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 444.* Previous: [443. Diagnostics](diagnostics.md) · [Contents](../README.md) · Next: [445. Recovering an unbootable system](recovering-an-unbootable-system.md)

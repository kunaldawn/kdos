# Screen lock

The [screen lock]{.idx} is Plasma's: KWin's screen locker, [kscreenlocker]{.idx}, covers every
output and asks for the account's password before the session shows again. This page covers when
it locks by itself, the PAM stacks it checks a password against, locking by hand, and what to do
when the lock screen will not unlock. Locking, idle and power together are described in
[Locking, idle and power](../../kdos/02-user-guide/desktop.md); this page gives the procedures.

## Installation

| Port | Provides |
|---|---|
| [kscreenlocker](#port-kscreenlocker) | The locker KWin runs, its greeter `kscreenlocker_greet`, the Screen Locking page of System Settings, and the PAM stacks `kde` and `kde-fingerprint` |
| [kwin](#port-kwin) | Holds the locked state: while the session is locked KWin shows only the greeter, and a greeter that dies leaves the screen locked rather than open |
| [powerdevil](#port-powerdevil) | The idle timers that dim and blank the screen, and the lock before a suspend |
| [elogind](#port-elogind) | `PrepareForSleep`, on which the locker locks before the machine sleeps, and `loginctl lock-session` |
| [pam](#port-pam) | `unix_chkpwd`, the setuid helper that reads `/etc/shadow` for a process that is not root |
| [fprintd](#port-fprintd) | The fingerprint service the `kde-fingerprint` stack asks; see [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md) |

The locker runs inside the session and is not a service. The live medium and an installed system
lock alike, except that neither the live medium nor a virtual machine locks on a timer.

## Configuration

### When the screen locks

KDOS's defaults, from `fs/usr/share/kdos/xdg/kscreenlockerrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/kscreenlockerrc lines=21-32 pin=6e2969d877d3 -->

So the screen locks after 10 minutes without input, a key or a move within 5 seconds of the lock
unlocks with no password, and the machine always resumes locked. The screen itself dims at 5
minutes and turns off at 15 on mains power, which is PowerDevil's, from
`fs/usr/share/kdos/xdg/powerdevilrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/powerdevilrc lines=17-19 pin=cfea8a5afb15 -->

In a virtual machine and on the live medium an overlay directory goes first in
`XDG_CONFIG_DIRS` and turns the timed lock off, because over a viewer a desktop that locked
itself looks like one that stopped, and the live account has no password to type; `Meta+L` and
the lock on resume still work. The zero timeout is what makes System Settings → **Screen Locking**
read "Never" there, rather than the 10 minutes the overlay would otherwise inherit. From
`fs/usr/share/kdos/xdg-vm/kscreenlockerrc`:

<!-- kfs:include fs/usr/share/kdos/xdg-vm/kscreenlockerrc lines=20-22 pin=327a7209a464 -->

A person's choices on System Settings → **Screen Locking** are saved in
`~/.config/kscreenlockerrc` and outrank all of these.

### The PAM stacks

The greeter checks a password against `/etc/pam.d/kde` and, in parallel, a fingerprint against
`/etc/pam.d/kde-fingerprint`, from `ports/core/kde/kscreenlocker/build.sh`:

<!-- kfs:include ports/core/kde/kscreenlocker/build.sh lines=28-42 pin=fdaab59dcb2f -->

`system-auth` runs `pam_unix`, which, in a process that is not root, asks `unix_chkpwd` to check
the password against `/etc/shadow`. Smart card, face and FIDO unlocking are compiled out.

### An autologin that starts locked

On the encrypted single-account machine whose install asked for autologin, the session starts
behind the lock screen: the autologin's own configuration directory carries
`LockOnStart=true`. See [Login and autologin](login-and-autologin.md#the-pam-stacks).

## Usage

### Locking by hand

- `Meta+L`, or the Lock button in the Application Launcher's Leave row.
- From a terminal: `loginctl lock-session`, which asks elogind to lock the current session, or
  `qdbus6 org.freedesktop.ScreenSaver /ScreenSaver Lock`.

### Unlocking

Type the account's password and press `Enter`, or touch an enrolled finger. The greeter shows the
clock and the media controls; **Switch User** goes back to SDDM's greeter for another account
while this session stays locked.

### Changing when the screen locks

1. System Settings → **Screen Locking**.
2. Set **Lock screen automatically** to the minutes you want, or turn it off; set **Delay before
   password required** for the grace; turn **Lock after waking from sleep** on or off.
3. **Apply**. It applies at once.

### Keeping the screen on for a while

The battery and brightness applet in the system tray has **Manually block sleep and screen
locking**, which holds both until it is turned off. A video player or a presentation does the same
while it plays, through the inhibition interface; the applet lists who holds one.

## Tips

- The lock is drawn by KWin, so a lock screen that seems frozen is a greeter that died. KWin keeps
  the session locked and offers to restart the greeter; press a key or move the pointer.
- The lock screen's wallpaper follows the accent: `kdos theme` moves it to the chosen accent's
  wallpaper while it still names a KDOS one.
- `loginctl lock-sessions`, as root, locks every session on the machine, for example before
  handing a shared machine over.

## Troubleshooting

**The correct password is refused.** Log in on `tty2` with the same password. A refusal there too
means the password is not what you think. If `tty2` accepts it, check the helper:

```bash
ls -l /usr/sbin/unix_chkpwd     # -rwsr-xr-x root: setuid
sudo grep -E 'kscreenlocker|unix_chkpwd' /var/log/messages | tail
```

Without the setuid bit no process but root can check a password, and every unlock fails.

**The lock screen will not go away.** Unlock it from another console:

1. `Ctrl+Alt+F2`, log in as the same user.
2. `loginctl list-sessions` to find the locked graphical session's id, then
   `loginctl unlock-session <id>`.
3. `Ctrl+Alt+F1`.

**The screen does not lock after the timeout.** Something holds an inhibition: open the battery
and brightness applet to see which application. A virtual machine and the live medium do not lock
on a timer by design; `echo "$XDG_CONFIG_DIRS"` starts with an overlay there.

**The machine resumes unlocked.** `LockOnResume` was turned off on the Screen Locking page; turn
**Lock after waking from sleep** back on.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [Power management](power-management.md): idle dimming, suspend and the lid
- [Login and autologin](login-and-autologin.md): the greeter, and an autologin that starts locked
- [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md): enrolling a finger
- [Security model](../../kdos/03-architecture/security-model.md): the setuid helpers and PAM

<!-- book-nav -->
---

*Part XVI. System administration, chapter 406.* Previous: [405. Login and autologin](login-and-autologin.md) · [Contents](../README.md) · Next: [407. Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md)

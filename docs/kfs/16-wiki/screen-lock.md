# Screen lock

The [lock screen]{.idx} of KDOS is [`kdos-lock`]{.idx}, a Wayland client that covers every output and asks
for the password of the account that runs the session. This page covers what installs it, the
settings that start it on idle and on the [lid]{.idx}, the ways to lock by hand, keeping the screen awake,
and how to get back into a session whose lock refuses every password.

## Installation

[kdos-lock](#port-kdos-lock), installed by `50_desktop`, builds two programs:

| Program | Mode | Is |
|:--|:--|:--|
| `/usr/bin/kdos-lock` | 0755 | The lock screen, an `ext-session-lock-v1` client |
| `/usr/bin/kdos-checkpass` | 4755, owned by root | The password check the lock screen runs. It reads [`/etc/shadow`]{.idx}, which only root can read |

Both are on every image, live and installed. There is no service script: `kdos-lock` runs inside
the desktop session, started by the compositor's idle policy, by a key, by a menu row or by
`kdos-power suspend`. The compositor, [kdos-comp](#port-kdos-comp), holds the locked state; the
lock program only draws it and checks the password. See
[kdos-lock](../../kdos/04-programs/daemons.md#kdos-lock) and
[Who owns the locked state](../../kdos/04-programs/kdos-comp.md#who-owns-the-locked-state).

The password that unlocks is the account's own: `kdos` on the live medium, and the one given to the
installer on an installed system.

## Configuration

### ~/.config/kdos/comp.conf

The idle timers and the lid are compositor settings, per account. Every account's copy starts as
`/etc/skel/.config/kdos/comp.conf`, from `fs/etc/skel/.config/kdos/comp.conf` in the tree, with
every key commented out at its default:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=205-215 pin=5cbe5fe7b46e -->

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=222-232 pin=fb37a46b4d0b -->

The keys are applied when the compositor reloads; their ranges are in
[Keys applied on reload](../../kdos/04-programs/kdos-comp.md#keys-applied-on-reload). In a
virtual machine the three timers default to `0` until any one of them is set in the file, and
`lid_close` defaults to `off` until it is set; see [In a virtual machine](../../kdos/04-programs/kdos-comp.md#in-a-virtual-machine).

### ~/.config/kdos-comp/rc.xml

The keys that lock, start the screensaver and keep the screen awake are bindings in the
compositor's `rc.xml`, from `fs/etc/skel/.config/kdos-comp/rc.xml`:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=132-133 pin=84ab2c338155 -->

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=313-315 pin=b8a46f76234d -->

`kdos toggle stay-awake` writes a state file under `~/.local/state/kdos/toggles/`, which the
compositor reads each time it re-arms its idle timer; see
[kdos toggle](../../kdos/04-programs/kdos-command.md#kdos-toggle).

### /etc/shadow

[`kdos-checkpass`]{.idx} reads the caller's hash from `/etc/shadow` directly, with no PAM and no
name-service module. Nothing about the lock screen is configured there; what matters is that the
account has a password. An account with an empty or locked hash (`!` or `*`) cannot unlock the lock
screen at all. The checker's contract is in
[kdos-checkpass](../../kdos/03-architecture/security-model.md#kdos-checkpass).

## Usage

### Locking by hand

Any of these starts `kdos-lock`:

1. Press Super+L.
2. In the Start menu, click the first of the five power icons at the bottom right (pointing at it
   shows `Lock Screen` in the search field), or type `lock` and choose `Lock Screen` from the
   results; see
   [kdos-start](../../kdos/04-programs/kdos-shell.md#kdos-start).
3. Right-click the desktop and choose **Lock Screen**; see
   [kdos-desk](../../kdos/04-programs/kdos-shell.md#kdos-desk).
4. From a terminal in the session:

   ```bash
   kdos-lock
   ```

   It prints `locked` once the compositor confirms the lock, and exits 0 when the session is
   unlocked. It exits 1 with `kdos-lock: cannot create a lock surface — is kdos-comp running?`
   when there is no compositor to lock, and with
   `kdos-lock: the session is already locked by another client` when another lock program holds
   the session.

The lock draws one box on the first output, under the KDOS logo (the wordmark and the penguin)
when the screen has room for both: `KDOS` and the time, the account as `<user>@<host>`, a `password` field that shows a dot per
character typed, and `Enter to unlock`. Every other output is covered in the theme's background
colour. To unlock, type the account's password and press Enter. Esc, or a left or right click,
clears the field; the pointer does nothing else. Ctrl and Alt chords are not typed into the field,
and it holds at most 255 bytes, after which further keys are ignored.

A wrong password turns the box red with `Wrong password`. `Cannot check the password`, also in
red, means the checker could not tell either way; see Troubleshooting. Until the compositor has
confirmed the lock the field takes no keys: a key typed then shows `locking...` and is discarded.

### Locking on idle

1. Open `~/.config/kdos/comp.conf` in an editor.
2. Uncomment and set the timers, in seconds of no input. Each is measured from the last input, not
   from the step before it, and `0` turns a step off:

   ```text
   idle_dim  = 300
   idle_lock = 600
   idle_off  = 900
   ```

3. Reload the compositor:

   ```bash
   kdos-comp -r
   ```

Ten seconds before the lock, a notification says `Locking soon`; any input then keeps the session
unlocked. Input after the dim or the screen-off step brings the screen back and never unlocks.

### Locking when the lid closes

1. Set `lid_close` in `~/.config/kdos/comp.conf`:

   | Value | Closing the lid |
   |:--|:--|
   | `suspend` | Runs `kdos-power suspend`, which locks first and then suspends |
   | `lock` | Locks and powers the screens off |
   | `off` | Powers the screens off and does not lock |

2. Reload the compositor as above.

Opening the lid powers the screens on and never unlocks. The rules are in
[The lid](../../kdos/04-programs/kdos-comp.md#the-lid).

### Locking before a suspend

`kdos-power suspend` locks the session before the machine sleeps, so a resume shows the lock
screen. The order and the two-second wait are in
[Suspend locks the screen first](../../kdos/04-programs/daemons.md#suspend-locks-the-screen-first).
To suspend without locking:

```bash
kdos-power --no-lock suspend
```

### Keeping the screen awake

1. Press Super+Ctrl+I, or run `kdos toggle stay-awake`, which prints `stay-awake on`. The idle
   policy stops: no dim, no lock and no screen-off.
2. Press it again, or run the command again (`stay-awake off`), to let the idle policy run.

`kdos toggle` with no argument lists every switch and its state, as in
`stay-awake     off  never save, lock or blank on idle`.

A program that holds an idle inhibitor, such as a video player, has the same effect for as long as
it holds one.

## Tips

- The screensaver is a separate program, `kdos-saver`, started with Super+Shift+L. It does not
  lock; the idle policy never starts it. The first key, click or touch closes it, and that press
  reaches no window.
- `kdos why kdos-lock` prints the design notes about the lock screen, among them why a crashed lock
  program leaves the session locked.
- A lock started from a terminal outside the session needs that session's `WAYLAND_DISPLAY`; from
  `tty2` see the last item under Troubleshooting.
- Change the live medium's password before relying on the lock: on the live medium anyone who
  knows the default `kdos` unlocks it. `passwd` changes it for the session.

## Troubleshooting

**The lock screen answers `Cannot check the password` to the right password.**
`kdos-checkpass` has lost its setuid bit. It then cannot read `/etc/shadow`, answers "could not
tell" to every attempt, and the lock cannot be opened. (`Wrong password` is a different answer:
the checker read the hash and the password did not match it.) Ctrl+Alt+F2 still switches to
`tty2`: the compositor handles Ctrl+Alt+F1 to F12 before it looks at whether the session is
locked. Log in there and run `kdos doctor`; its Desktop section checks the bit,
`[warn] kdos-checkpass is not setuid root — every password will be refused (fix: chown root and chmod 4755)`,
and the repair is:

```bash
sudo chown root /usr/bin/kdos-checkpass
sudo chmod 4755 /usr/bin/kdos-checkpass
```

Then return with Alt+F1 and type the password again.

**A wrong password takes about two seconds to be refused.**
That is intended. `kdos-checkpass` waits one second before it answers a wrong password (a wrong
one takes about 1005 ms, a right one under 10 ms), and the lock screen pauses a little over a
second more before it takes the next attempt.

**The account has no password, and the lock cannot be opened.**
An empty or locked hash never unlocks: the lock shows `Cannot check the password`. The lock
discards the checker's own message; run the checker by hand from `tty2`, as the same account, to
read it:

```bash
printf '%s' 'the password' | kdos-checkpass; echo $?
```

It prints `kdos-checkpass: the account has no password — it cannot be unlocked this way` and `2`.
`0` is the right password, `1` a wrong one. Set a password for the account
as root: `sudo passwd <user>` from an account in `wheel`, or `passwd <user>` logged in as root.

**The screen never locks on idle.**
Check, in this order: that `idle_lock` is set and not `0` in `~/.config/kdos/comp.conf` (in a
virtual machine it defaults to `0`); that `stay-awake` is off (`kdos toggle` lists every switch and its
state); and that no program holds an idle inhibitor, as a video player does while it plays.

**`kdos-power suspend` prints `no lock confirmation within 2s — suspending anyway`.**
`kdos-lock` did not confirm the lock in time, and the machine suspended unlocked. Run `kdos-lock`
from a terminal in the session and read what it prints on standard error.

**`kdos-lock` prints `the session is already locked by another client` and exits.**
A lock program already holds the session, and the compositor refuses a second one. The lock that
is already on screen is the one to type the password into.

**`kdos-lock` prints `cannot create a lock surface — is kdos-comp running?`.**
It found no compositor that offers session locking: it was run outside the desktop session, or
with no `WAYLAND_DISPLAY` naming the session's display. Nothing was locked.

**The screens stay covered and show no password field.**
The lock program has died and the compositor keeps the session locked, as it should: every output
stays an empty black cover. A new lock client can take over. Log in on `tty2` as the same account
and start one against the session's display:

```bash
export WAYLAND_DISPLAY=$(ls "$XDG_RUNTIME_DIR" | grep '^wayland-' | grep -v '\.lock$' | head -1)
kdos-lock &
```

Return to `tty1` with Alt+F1. The new `kdos-lock` prints `locked`, draws the password box over the
black cover, and unlocks the session with the account's password.

## Known gaps

- [Nothing starts the screensaver on idle](../../kdos/06-reference/known-gaps.md#nothing-starts-the-screensaver-on-idle)
- [A fingerprint unlocks sudo and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else):
  the lock screen asks for no finger
- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)

## See also

- [Idle, dim, lock and lid](../../kdos/04-programs/kdos-comp.md#idle-dim-lock-and-lid): the
  compositor's idle policy
- [kdos-lock](../../kdos/04-programs/daemons.md#kdos-lock) and
  [kdos-checkpass](../../kdos/03-architecture/security-model.md#kdos-checkpass): the lock program
  and its checker
- [~/.config/kdos/comp.conf](../../kdos/06-reference/configuration.md#configkdoscompconf): every
  compositor key
- [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor): the setuid check
- [Login and autologin](login-and-autologin.md), [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md),
  [Users and groups](users-and-groups.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 382.* Previous: [381. Login and autologin](login-and-autologin.md) · [Contents](../README.md) · Next: [383. Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md)

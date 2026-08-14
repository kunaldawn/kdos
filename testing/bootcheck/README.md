# testing/bootcheck — driving a booted KDOS from a script

`preflight.sh` proves the wiring, `selftest.sh` proves the decisions, and
neither of them has ever seen a session. Both were clean on the day the desktop
opened every window underneath its own panel, drew the launcher's selected row
as a solid green block with the text punched out of it, and parked an empty
notification rectangle in the middle of the screen. Those are screen bugs, and
the only thing that finds a screen bug is a screen.

This is the plumbing for the third gate. It boots the ISO with **no display**,
puts the serial console and the qemu monitor on unix sockets, and gives a script
three verbs: run a command in the guest, type into the VT, and take a picture.

```sh
testing/bootcheck/boot.sh soft                  # boot; pixman, like `make run`
testing/bootcheck/guest.py 'kdos doctor'        # run on the serial root shell
testing/bootcheck/type.py 'kdos-desktop\n'      # type into tty1
echo "screendump /tmp/x.ppm" > /tmp/kdos-bootcheck/mon.fifo
kill $(cat /tmp/kdos-bootcheck/qemu.pid)
```

| | |
|---|---|
| `boot.sh [soft\|gl]` | starts qemu detached; `soft` is the pixman renderer `make run` gives, `gl` is virgl for the CRT pass |
| `sock.py <sock> <name>` | owns one qemu socket: everything it reads goes to `<name>.log`, everything written to `<name>.fifo` goes out |
| `guest.py <cmd…>` | runs a command on the serial console and prints only its output; `KQ_TIMEOUT` in seconds |
| `type.py '<text>'` | types into the VT via monitor `sendkey`; `\n` is Enter |

Four things it exists to encode, each of which cost a debug cycle:

- **One owner per socket.** A background reader polling the serial socket while
  something else writes to it desynchronises the console: replies land in the
  wrong reader and the shell appears to hang. `sock.py` is the only thing that
  touches a socket; everyone else uses the log and the fifo.
- **The command's output is bracketed, not guessed at.** `guest.py` wraps the
  command in `echo @B…@` / `echo @E…@$?` and reads between the LAST pair — the
  console echoes the command line itself, so a single marker matches twice and
  the first match is the echo.
- **The sockets are not beside the scripts.** A unix socket path is capped at
  108 bytes and qemu refuses a longer one; `$KDOS_BOOTCHECK_RUN` defaults to
  `/tmp/kdos-bootcheck` for that reason alone.
- **`screendump` works because the renderer is software.** Under
  `-display egl-headless` it answers "no surface" and the picture has to come
  from the VNC framebuffer instead. `soft` is the mode to assert on.

Two facts about the guest itself: **the serial console is a root login** (tty2
and the serial port are root per `fs/etc/inittab`) while **tty1 autologins as
`kdos`**, and the desktop is started by hand from tty1 — so `type.py` is how the
session gets started and `guest.py` is how it gets inspected. Anything run as
`kdos` from the serial side needs `su - kdos` (a LOGIN shell: a plain `su kdos`
leaves podman resolving `HOME` to `/`) plus `XDG_RUNTIME_DIR`,
`WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` in its environment.

**What is missing is the assertions** — see `KDOS-ROADMAP.md` Wave R, item R5,
which lists the first eight and what each of them would have caught.

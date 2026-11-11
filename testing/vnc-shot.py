#!/usr/bin/env python3
"""Boot the ISO headless, drive it over serial, and photograph the screen.

WHY THIS EXISTS. A desktop is checked by looking at it: a panel on the wrong
edge, a window that never maps, a greeter that draws nothing and a lock screen
in the wrong colours are invisible to a compiler and to every unit test. This
is how the screen gets looked at without a human in front of it.

    testing/vnc-shot.py                       boot, wait for Plasma, shoot
    testing/vnc-shot.py --keys meta_l         press something first
    testing/vnc-shot.py --cmd 'konsole &'     run something in the session first
    testing/vnc-shot.py --until greeter       stop at the SDDM greeter instead
    testing/vnc-shot.py --keep                leave the VM up afterwards

`--keys`, `--cmd`, `--root-cmd`, `--sleep`, `--shot` and the other steps are
ONE ORDERED LIST in the order they appear on the command line, so a boot can
photograph a dozen surfaces instead of one:

    testing/vnc-shot.py --size 1920x1080 --gl --dismiss-picker \
        --shot bare.ppm --keys meta_l --shot kickoff.ppm --keys esc

THE BOOT IT WAITS FOR. inittab starts SDDM on tty1 once rcS has returned, and
the live medium's /etc/sddm.conf.d/kde_settings.conf logs `kdos` in with no
greeter; an installed disk shows the greeter. Before any step runs the run
passes a READINESS GATE, polled in the guest over the serial console: for
`--until plasma` (the default) org.kde.plasmashell is owned on the user's bus,
/run/user/<uid>/bus, and KSplash has reached its last stage (or is gone); for
`--until greeter` sddm-greeter and its KWin run as the `sddm` account. A gate
that is not passed within `--gate-timeout` FAILS THE RUN with the processes,
SDDM's log and the session log printed, instead of photographing a black
screen; `--until shell` skips it, for a run whose root script is the test.

`--gl` asks for virtio-vga-gl on an egl-headless display, so KWin composites
with OpenGL on the host's GPU through virgl. Without it KWin renders on
llvmpipe, which it treats as software GL, and an effect that refuses software
GL — Phosphor mode's kdos_crt — is not loaded and not in the picture. The
rig image (testing/Dockerfile.qemu) carries QEMU's OpenGL module and
virglrenderer for it; the container reaches the host GPU only when it is run
with --gpus all and --device /dev/dri, as testing/session-cost.sh --gl runs
it. Without those the only device inside the container is llvmpipe, which the
guest never sees and cannot be told apart from a slow one. `--gl` gives the guest GL and no Vulkan:
`--venus` adds the Vulkan capset, and WITHOUT IT every Vulkan tool in the guest
measures lavapipe on the CPU without saying so. On an NVIDIA host `--venus`
itself is measured dead at both ends — the flag's own help says how.

Three things it has to get right, each also stated as a trap in CLAUDE.md's
rig section:

  - `screendump` over the monitor socket answers "no surface" under a GL
    display, so the framebuffer is read over RFB instead. The handshake's
    SetEncodings message is `type(1) pad(1) count(2)` followed by count
    int32s — an extra padding field desyncs the stream and every later read
    blocks forever.
  - The serial console is a ROOT login on this image, and driving the session
    means `su - kdos -c` — a LOGIN shell, because a plain `su kdos -c` leaves
    podman resolving HOME to `/` and every call fails. `--cmd` then takes the
    rest of its environment from the running plasmashell, so a program it
    starts is on the session's bus and display.
  - Keys go to the ACTIVE VT. tty1 is KWin's, through logind, so `--keys`,
    `--chord` and `--type` reach the focused window of the session; Ctrl+Alt+F2
    is the text console, and `--console-cmd` logs in there.
"""

import argparse
import base64
import json
import os
import re
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ISO = os.path.join(REPO, "build", "iso-build", "kdos.iso")


def wait_for(path, timeout=30):
    for _ in range(timeout * 10):
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


class Serial:
    """The serial console, as a unix socket."""

    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.s.settimeout(0.5)
        self.buf = b""

    def pump(self):
        try:
            d = self.s.recv(65536)
            if d:
                self.buf += d
        except socket.timeout:
            pass
        except OSError:
            pass
        return self.buf

    def expect(self, needle, timeout=120):
        deadline = time.time() + timeout
        needle = needle.encode() if isinstance(needle, str) else needle
        while time.time() < deadline:
            if needle in self.buf:
                return True
            self.pump()
        return False

    def send(self, line, pace=0.3):
        self.s.sendall(line.encode() + b"\n")
        time.sleep(pace)

    def tail(self, n=2000):
        return self.buf[-n:].decode("utf-8", "replace")


class Monitor:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)
        self.s.settimeout(1.0)
        time.sleep(0.3)
        self.drain()

    def drain(self):
        try:
            while True:
                if not self.s.recv(65536):
                    return
        except (socket.timeout, OSError):
            pass

    def cmd(self, c):
        self.s.sendall(c.encode() + b"\n")
        time.sleep(0.4)
        self.drain()

    def type(self, text, ret=True):
        """Type on the active VT, one `sendkey` per character.

        `ret` sends Return at the end, which is what a command line wants and
        what a FIELD does not: a search box, a rename or a filter is answered
        while it is being typed, and a Return there has already moved on to
        whatever the first match does.

        The keys reach whatever holds the active VT: on tty1 that is KWin, and
        KWin hands them to the focused window — the greeter's password field,
        KRunner, a Konsole — so a run that types must first put the focus where
        the text belongs. On tty2 they reach the text console's login. Nothing
        about the display is driven from the serial root shell instead: a key
        sent there reaches a shell, not a window.
        """
        # A character with no qemu keyname is sent VERBATIM, which the monitor
        # rejects — silently, from this side — so the line arrives with that
        # character simply missing. `clear; ls` typed as `clear ls` runs clear
        # with an argument and photographs an empty screen. Anything reachable
        # on a US layout is named here.
        names = {" ": "spc", "-": "minus", "=": "equal", "[": "bracket_left",
                 "]": "bracket_right", ";": "semicolon", "'": "apostrophe",
                 "\\": "backslash", ",": "comma", ".": "dot", "/": "slash",
                 "`": "grave_accent",
                 "_": "shift-minus", "+": "shift-equal", "{": "shift-bracket_left",
                 "}": "shift-bracket_right", ":": "shift-semicolon",
                 '"': "shift-apostrophe", "|": "shift-backslash",
                 "<": "shift-comma", ">": "shift-dot", "?": "shift-slash",
                 "~": "shift-grave_accent", "!": "shift-1", "@": "shift-2",
                 "#": "shift-3", "$": "shift-4", "%": "shift-5",
                 "^": "shift-6", "&": "shift-7", "*": "shift-8",
                 "(": "shift-9", ")": "shift-0"}
        for ch in text:
            if ch.isupper():
                key = "shift-" + ch.lower()
            else:
                key = names.get(ch, ch)
            self.cmd("sendkey " + key)
            time.sleep(0.08)
        if ret:
            self.cmd("sendkey ret")


def rfb_pointer(host, port, moves):
    """Put the pointer somewhere, and optionally click.

    WHY OVER RFB RATHER THAN THE MONITOR. qemu's `mouse_move` is RELATIVE and
    this image presents a usb-tablet, which is ABSOLUTE — the two disagree
    about what a coordinate means and the pointer ends up somewhere nobody
    asked for. RFB's PointerEvent is absolute by definition and lands on the
    pixel it names, which is the whole point: HOVER is a state half of every
    desktop's chrome has, and a photograph of it is the only way to see it.

    `moves` is a list of (x, y, button_mask). A click is three of them —
    move, press, release — because a press with no move first lands wherever
    the pointer happened to be.
    """
    s, _w, _h, _pf = rfb_handshake(host, port)
    for x, y, mask in moves:
        s.sendall(struct.pack(">BBHH", 5, mask, x, y))
        s.sendall(struct.pack(">BBHHHH", 3, 1, 0, 0, 1, 1))
        time.sleep(0.15)
    # The server processes what it has been sent before the socket closes, and
    # a close with the last event still in flight loses it.
    time.sleep(0.5)
    s.close()


# THE SOCKET A HELD DRAG LIVES ON.
#
# A BUTTON IS DOWN ONLY WHILE ITS CLIENT IS CONNECTED. qemu's VNC server
# releases every button when a client disconnects, so a press sent down one
# connection and a move sent down the next is two clicks and no drag. The
# whole gesture has to travel one socket, which means the socket outlives the
# step that opened it.
#
# WHICH IS WHAT `--press` EXISTS FOR. `--drag` sends press, motion and release
# in one call and nothing can be photographed between them, so a bug that only
# exists WHILE the button is down — a window whose frame is redrawn on every
# motion event — is invisible to it. Held open, a `--shot` in the middle
# photographs exactly that.
_held = {"sock": None, "mask": 0}


def rfb_hold(host, port, x, y, mask):
    """Move the pointer, with `mask` held, on the drag's own connection."""
    if _held["sock"] is None:
        _held["sock"] = rfb_handshake(host, port)[0]
    s = _held["sock"]
    s.sendall(struct.pack(">BBHH", 5, mask, x, y))
    s.sendall(struct.pack(">BBHHHH", 3, 1, 0, 0, 1, 1))
    _held["mask"] = mask
    time.sleep(0.15)


def rfb_sweep(host, port, x0, y0, x1, y1, n, mask, gap):
    """A drag at something like a real mouse's REPORT RATE.

    `--drag` and `--press` space their events 150 ms apart, which is about six
    a second; a mouse on a desk sends between 125 and 1000. A defect that only
    appears when the events arrive faster than the session can answer them is
    invisible to both, and "I cannot reproduce it" then means "my hand is
    slower than theirs", not "it is not there". One connection, one press, `n`
    interpolated moves `gap` seconds apart, one release.
    """
    s, _w, _h, _pf = rfb_handshake(host, port)

    def at(x, y, m):
        s.sendall(struct.pack(">BBHH", 5, m, x, y))

    at(x0, y0, 0)
    time.sleep(0.15)
    at(x0, y0, mask)
    time.sleep(0.05)
    for i in range(1, n + 1):
        at(x0 + (x1 - x0) * i // n, y0 + (y1 - y0) * i // n, mask)
        if gap:
            time.sleep(gap)
    at(x1, y1, mask)
    time.sleep(0.05)
    at(x1, y1, 0)
    # The framebuffer request is what makes the server drain the queue before
    # the socket closes — see rfb_pointer.
    s.sendall(struct.pack(">BBHHHH", 3, 1, 0, 0, 1, 1))
    time.sleep(0.5)
    s.close()


def rfb_drop(host, port, x, y):
    """Let go, and close the drag's connection. A release with no press is a
    no-op rather than an error: a run that ends mid-drag still has to tidy
    up, and the tidy-up cannot be the thing that fails it."""
    s = _held["sock"]
    if s is None:
        return
    s.sendall(struct.pack(">BBHH", 5, 0, x, y))
    s.sendall(struct.pack(">BBHHHH", 3, 1, 0, 0, 1, 1))
    time.sleep(0.5)
    s.close()
    _held["sock"] = None
    _held["mask"] = 0


# The keysyms a chord can name. X11's numbering, which is what RFB carries.
CHORD_KEYS = {
    "super": 0xffeb, "meta": 0xffeb, "shift": 0xffe1, "ctrl": 0xffe3,
    "alt": 0xffe9, "space": 0x0020, "tab": 0xff09, "ret": 0xff0d,
    "esc": 0xff1b, "up": 0xff52, "down": 0xff54, "left": 0xff51,
    "right": 0xff53, "home": 0xff50, "end": 0xff57, "backspace": 0xff08,
    "delete": 0xffff, "print": 0xff61,
}
for _i in range(1, 13):
    CHORD_KEYS["f%d" % _i] = 0xffbd + _i


def rfb_chord(host, port, spec):
    """Hold the modifiers, tap the key, let go — over RFB.

    WHY NOT `sendkey`. qemu's monitor presses and releases a whole combination
    in one command, and a chord with TWO modifiers in it does not arrive:
    KWin sees Ctrl+Alt+F2 or Meta+Shift+T as nothing at all. RFB's KeyEvent is
    one key and one direction, so the modifiers are genuinely DOWN while the
    key is tapped — which is what a keyboard does and what xkb's modifier state
    is built to see. Ctrl+Alt+F<n> reaches xkb as XF86Switch_VT_<n>, which KWin
    turns into a logind VT switch.
    """
    names = [n.strip().lower() for n in spec.split("+") if n.strip()]
    if not names:
        return
    shifted = "shift" in names
    syms = []
    for n in names:
        if n in CHORD_KEYS:
            syms.append(CHORD_KEYS[n])
        elif len(n) == 1:
            # THE SHIFTED KEYSYM WHERE SHIFT IS HELD. qemu's VNC input
            # produces the keysym it was asked for and adjusts the shift state
            # to do it — so asking for a lowercase letter with Shift down makes
            # it RELEASE Shift, and the chord arrives as the bare letter typed
            # into whatever had the focus.
            syms.append(ord(n.upper() if shifted else n))
        else:
            raise SystemExit("--chord: no keysym named %r" % n)
    s, _w, _h, _pf = rfb_handshake(host, port)
    for sym in syms:
        s.sendall(struct.pack(">BBHI", 4, 1, 0, sym))
        time.sleep(0.05)
    for sym in reversed(syms):
        s.sendall(struct.pack(">BBHI", 4, 0, 0, sym))
        time.sleep(0.05)
    # A FRAMEBUFFER REQUEST AFTER THE RELEASE, for rfb_pointer's reason: the
    # server processes a client's messages in order, and closing the socket
    # before it has read them throws the chord away.
    s.sendall(struct.pack(">BBHHHH", 3, 1, 0, 0, 1, 1))
    time.sleep(0.2)
    s.close()


def rfb_handshake(host, port):
    """Connect and negotiate; returns (socket, w, h, pixel-format bytes)."""
    s = socket.create_connection((host, port), timeout=20)
    s.settimeout(30)

    def recv_exact(n):
        out_b = b""
        while len(out_b) < n:
            chunk = s.recv(n - len(out_b))
            if not chunk:
                raise RuntimeError("VNC closed mid-read")
            out_b += chunk
        return out_b

    version = recv_exact(12)
    if not version.startswith(b"RFB "):
        raise RuntimeError("not a VNC server: %r" % version)
    s.sendall(b"RFB 003.008\n")

    nsec = recv_exact(1)[0]
    sectypes = recv_exact(nsec)
    if 1 not in sectypes:
        raise RuntimeError("VNC wants authentication; this rig runs without")
    s.sendall(bytes([1]))
    res = struct.unpack(">I", recv_exact(4))[0]
    if res != 0:
        raise RuntimeError("VNC security handshake failed")

    s.sendall(bytes([1]))                      # shared
    w, h = struct.unpack(">HH", recv_exact(4))
    pf = recv_exact(16)
    namelen = struct.unpack(">I", recv_exact(4))[0]
    recv_exact(namelen)
    # SetEncodings: type(1) pad(1) count(2), then count int32s. The extra pad
    # everybody adds here is what desyncs the stream.
    s.sendall(struct.pack(">BBH", 2, 0, 1) + struct.pack(">i", 0))
    return s, w, h, pf


def rfb_shot(host, port, out):
    """One framebuffer, over RFB, Raw encoding, written as a PPM.

    No compression is negotiated on purpose: a raw 1280x800 frame is 4 MB over
    a loopback socket, and a decoder for tight/zrle here would be more code
    than the thing it is testing.
    """
    s, w, h, pf = rfb_handshake(host, port)

    def recv_exact(n):
        out_b = b""
        while len(out_b) < n:
            chunk = s.recv(n - len(out_b))
            if not chunk:
                raise RuntimeError("VNC closed mid-read")
            out_b += chunk
        return out_b

    bpp, depth, big_endian, true_colour = pf[0], pf[1], pf[2], pf[3]
    rmax, gmax, bmax = struct.unpack(">HHH", pf[4:10])
    rsh, gsh, bsh = pf[10], pf[11], pf[12]
    if bpp not in (8, 16, 32) or not true_colour:
        raise RuntimeError("unhandled pixel format bpp=%d true=%d"
                           % (bpp, true_colour))

    # FramebufferUpdateRequest: type, incremental, x, y, w, h
    s.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, w, h))

    rows = [bytearray(w * 3) for _ in range(h)]
    got = 0
    deadline = time.time() + 30
    while got < w * h and time.time() < deadline:
        msg = recv_exact(1)[0]
        if msg != 0:
            # A bell or a clipboard message: skip its body and keep reading.
            if msg == 2:
                continue
            if msg == 3:
                recv_exact(3)
                n = struct.unpack(">I", recv_exact(4))[0]
                recv_exact(n)
                continue
            raise RuntimeError("unexpected RFB message %d" % msg)
        recv_exact(1)
        nrect = struct.unpack(">H", recv_exact(2))[0]
        for _ in range(nrect):
            rx, ry, rw, rh, enc = struct.unpack(">HHHHi", recv_exact(12))
            if enc != 0:
                raise RuntimeError("server used encoding %d, not Raw" % enc)
            data = recv_exact(rw * rh * (bpp // 8))
            step = bpp // 8
            for yy in range(rh):
                base = yy * rw * step
                row = rows[ry + yy]
                for xx in range(rw):
                    o = base + xx * step
                    if step == 4:
                        px = int.from_bytes(data[o:o + 4],
                                            "big" if big_endian else "little")
                    elif step == 2:
                        px = int.from_bytes(data[o:o + 2],
                                            "big" if big_endian else "little")
                    else:
                        px = data[o]
                    r = ((px >> rsh) & rmax) * 255 // rmax
                    g = ((px >> gsh) & gmax) * 255 // gmax
                    b = ((px >> bsh) & bmax) * 255 // bmax
                    j = (rx + xx) * 3
                    row[j] = r
                    row[j + 1] = g
                    row[j + 2] = b
            got += rw * rh
    s.close()

    with open(out, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        for row in rows:
            f.write(bytes(row))
    return w, h


# ── what runs in the guest ──────────────────────────────────────────────
#
# Each of these is sent down the serial console by guest_run() and run by
# root's shell there. They are POSIX sh, because the serial login is whatever
# shell root has, and they print what they found as well as deciding: a gate
# that fails says why on the same console the run prints.

# THE READINESS GATE. $1 the session account, $2 plasma or greeter, $3 the
# seconds it may take.
#
# PLASMA IS UP WHEN TWO THINGS HOLD. org.kde.plasmashell is owned on the user's
# bus — and that alone is not enough, because plasmashell takes its name early
# in main(), long before a panel is drawn. The shell says it has finished by
# calling org.kde.KSplash setStage("desktop"), the splash's last stage, so the
# gate watches for that call with dbus-monitor once the bus exists. A gate that
# started after the call has been made cannot see it, and then the splash
# having exited stands in for it: ksplashqml exits on its sixth stage or after
# its own 30-second timer, and a session configured with no splash has none.
#
# THE BUS IS ASKED AS THE USER. A session bus admits only the account that
# owns it — root's connection is dropped before Hello is answered — so every
# dbus-send and the monitor run under setpriv with the account's ids: no PAM,
# so no poll opens a login session of its own.
GATE_SH = r"""
set -u
user=$1 until=$2 limit=$3
uid=$(id -u "$user" 2>/dev/null) || { echo "KDOSGATE fail: no account $user"; exit 1; }
gid=$(id -g "$user")
as_user() { setpriv --reuid="$uid" --regid="$gid" --init-groups "$@"; }
home=$(awk -F: -v u="$user" '$1 == u { print $6 }' /etc/passwd)
bus=/run/user/$uid/bus
addr=unix:path=$bus
mkdir -p /run/kdos-rig
log=/run/kdos-rig/ksplash.log
: > "$log"
now() { read -r _t _ < /proc/uptime; echo "$_t"; }
t0=$(now)
since() { awk -v a="$t0" -v b="$(now)" 'BEGIN { printf "%.1f", b - a }'; }
over() { awk -v a="$t0" -v b="$(now)" -v l="$limit" 'BEGIN { exit !(b - a > l) }'; }
owned() {
	as_user dbus-send --bus="$addr" --print-reply=literal --dest=org.freedesktop.DBus \
		/org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner string:"$1" \
		2>/dev/null | grep -q true
}
greeter() { pgrep -u sddm -f '^[^ ]*sddm-greeter' >/dev/null 2>&1; }
diagnose() {
	echo "--- processes"
	pgrep -a 'sddm|kwin_wayland|plasmashell|ksplashqml|startplasma|plasma_session|ksmserver|kded6|kdos-dm|getty' 2>/dev/null
	if [ -r /run/kdos/kdos-dm.starts ]; then
		echo "--- kdos-dm starts (seconds since boot)"
		cat /run/kdos/kdos-dm.starts
	fi
	echo "--- /var/log/sddm.log"
	tail -n 40 /var/log/sddm.log 2>/dev/null
	if [ -n "$home" ] && [ -r "$home/.local/share/sddm/wayland-session.log" ]; then
		echo "--- $home/.local/share/sddm/wayland-session.log"
		tail -n 60 "$home/.local/share/sddm/wayland-session.log"
	fi
	echo "--- KSplash stages seen"
	grep -o 'string "[^"]*"' "$log" 2>/dev/null
}
mon=""
why=""
while :; do
	if over; then
		[ -n "$mon" ] && kill "$mon" 2>/dev/null
		case $until in
		plasma)
			if greeter; then
				echo "KDOSGATE note: the SDDM greeter is up — no autologin on this boot; use --until greeter and log in with --type"
			fi
			echo "KDOSGATE fail: no Plasma session for $user after ${limit}s"
			;;
		greeter) echo "KDOSGATE fail: no SDDM greeter after ${limit}s" ;;
		esac
		diagnose
		exit 1
	fi
	case $until in
	greeter)
		if greeter && pgrep -u sddm -x kwin_wayland >/dev/null 2>&1; then
			why="sddm-greeter and its KWin run as sddm"
			break
		fi
		;;
	plasma)
		if [ -z "$mon" ] && [ -S "$bus" ]; then
			setpriv --reuid="$uid" --regid="$gid" --init-groups dbus-monitor --address "$addr" \
				"type='method_call',interface='org.kde.KSplash',member='setStage'" \
				> "$log" 2>&1 &
			mon=$!
		fi
		if [ -S "$bus" ] && owned org.kde.plasmashell; then
			if grep -q 'string "desktop"' "$log" 2>/dev/null; then
				why="plasmashell owns its name and KSplash reached its last stage"
				break
			fi
			if ! pgrep -u "$uid" -x ksplashqml >/dev/null 2>&1; then
				why="plasmashell owns its name and no KSplash is running"
				break
			fi
		fi
		;;
	esac
	sleep 0.25
done
[ -n "$mon" ] && kill "$mon" 2>/dev/null
echo "KDOSGATE ok $(since)s: $why"
"""

# THE ENVIRONMENT A --cmd RUNS IN, sourced by the session user's login shell.
#
# A login shell reads the profile and nothing of the session: no
# WAYLAND_DISPLAY, no DISPLAY for Xwayland, no XDG_CONFIG_DIRS with KDOS's
# defaults in it, no XDG_SESSION_ID. Plasma has all of them, so they are read
# from the running plasmashell's own environment (KWin's when there is no
# shell yet) — what that process was started with is what anything it launches
# would get. Only the login's own identity is kept: HOME, USER, SHELL and the
# working directory are the login shell's. A value holding a newline arrives
# split, and its continuation is skipped unless it reads as NAME=value.
ENV_SH = r"""
_kdos_rig_pid=$(pgrep -u "$(id -u)" -x plasmashell 2>/dev/null | head -n 1)
[ -n "$_kdos_rig_pid" ] || _kdos_rig_pid=$(pgrep -u "$(id -u)" -x kwin_wayland 2>/dev/null | head -n 1)
if [ -n "$_kdos_rig_pid" ] && [ -r "/proc/$_kdos_rig_pid/environ" ]; then
	while IFS= read -r _kdos_rig_kv; do
		case $_kdos_rig_kv in
		*=*) ;;
		*) continue ;;
		esac
		case ${_kdos_rig_kv%%=*} in
		'' | [0-9]* | *[!A-Za-z0-9_]*) continue ;;
		HOME | PWD | OLDPWD | SHLVL | _ | USER | LOGNAME | SHELL | MAIL | TERM) continue ;;
		esac
		# shellcheck disable=SC2163 # the line is NAME=value: export it as given
		export "$_kdos_rig_kv"
	done <<KDOS_RIG_ENV
$(tr '\0' '\n' < "/proc/$_kdos_rig_pid/environ")
KDOS_RIG_ENV
fi
: "${XDG_RUNTIME_DIR:=/run/user/$(id -u)}"
: "${DBUS_SESSION_BUS_ADDRESS:=unix:path=$XDG_RUNTIME_DIR/bus}"
: "${WAYLAND_DISPLAY:=wayland-0}"
export XDG_RUNTIME_DIR DBUS_SESSION_BUS_ADDRESS WAYLAND_DISPLAY
unset _kdos_rig_pid _kdos_rig_kv
"""

# THE FIRST-RUN WINDOW. $1 the session account, $2 how many seconds to wait
# for it.
#
# plasma-welcome is the desk picker and the welcome tour, and its kded module
# opens it on every live session and on an account's first login — over
# whatever a run meant to photograph. It is closed the way its own Quit is:
# QCoreApplication.quit on its KDBusService name, which lets it write its
# settings; a window that will not quit is sent TERM. A run that does not see
# the window within the wait says so and carries on, because a second login
# and a session with its kded module removed never open it.
DISMISS_SH = r"""
set -u
user=$1 limit=$2
uid=$(id -u "$user" 2>/dev/null) || { echo "dismiss: no account $user"; exit 1; }
gid=$(id -g "$user")
addr=unix:path=/run/user/$uid/bus
n=0
pid=""
while [ "$n" -lt $((limit * 4)) ]; do
	pid=$(pgrep -u "$uid" -x plasma-welcome 2>/dev/null | head -n 1)
	[ -n "$pid" ] && break
	sleep 0.25
	n=$((n + 1))
done
if [ -z "$pid" ]; then
	echo "dismiss: no plasma-welcome window within ${limit}s"
	exit 0
fi
# The window maps after the process starts; a quit before it has is a quit of
# a window the run never saw.
sleep 2
setpriv --reuid="$uid" --regid="$gid" --init-groups \
	dbus-send --bus="$addr" --print-reply --dest=org.kde.plasma-welcome \
	/MainApplication org.qtproject.Qt.QCoreApplication.quit >/dev/null 2>&1
n=0
while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 20 ]; do
	sleep 0.25
	n=$((n + 1))
done
if kill -0 "$pid" 2>/dev/null; then
	kill -TERM "$pid" 2>/dev/null
	echo "dismiss: plasma-welcome ($pid) did not quit; sent TERM"
else
	echo "dismiss: plasma-welcome ($pid) closed"
fi
"""


def guest_run(ser, body, args=(), name="step", timeout=600):
    """Run a script inside the guest, as root, and return (output, status).

    There is no shared filesystem with the guest — the repo is not on the ISO
    unless KDOS_ISO_SOURCES was set — so the file travels down the serial
    console. It goes as BASE64 IN SHORT LINES for two separate reasons: a tty
    in canonical mode drops everything past its line limit, silently, so a
    script long enough to be worth keeping cannot be one line; and an encoded
    payload contains nothing the shell would act on before `base64 -d` hands it
    back, which a heredoc of arbitrary text does not promise.

    The END marker is echoed by the guest with the script's exit status after
    it, so what comes back is delimited by the guest's own output rather than
    by a timeout: a step that takes four minutes to install a pack is waited
    for rather than truncated. The empty quote inside the typed marker keeps a
    terminal's echo of the command line from ever reading as the guest's
    answer. `name` keeps two scripts of one run from overwriting each other.
    """
    if isinstance(body, str):
        body = body.encode()
    blob = base64.b64encode(body).decode()
    path = "/tmp/kdos-rig-%s" % name
    ser.send(": > %s.b64" % path)
    for i in range(0, len(blob), 512):
        ser.send("printf %%s '%s' >> %s.b64" % (blob[i:i + 512], path),
                 pace=0.05)
    ser.buf = b""
    argv = " ".join(shlex.quote(a) for a in args)
    ser.send("base64 -d %s.b64 > %s.sh; "
             "echo ===KDOSSCRIPT-''BEGIN===; sh %s.sh %s 2>&1; "
             "echo ===KDOSSCRIPT-''END=== rc=$?" % (path, path, path, argv))
    done = ser.expect("===KDOSSCRIPT-END===", timeout=timeout)
    # The status follows the marker on the same line, and may arrive in the
    # next read.
    status_re = re.compile(rb"===KDOSSCRIPT-END=== rc=\d+\s")
    settle = time.time() + 3
    while done and not status_re.search(ser.buf) and time.time() < settle:
        ser.pump()
    out = ser.buf.decode("utf-8", "replace")
    cut = out.rfind("===KDOSSCRIPT-BEGIN===")
    if cut >= 0:
        out = out[cut + len("===KDOSSCRIPT-BEGIN==="):]
    status = None
    m = re.search(r"===KDOSSCRIPT-END=== rc=(\d+)", out)
    if m:
        status = int(m.group(1))
        out = out[:m.start()]
    if not done:
        out += "\n[rig] the script never finished"
    return out.strip("\r\n"), status


class Step(argparse.Action):
    """Append (kind, value) to one ORDERED list shared by every action flag.

    ORDER ON THE COMMAND LINE IS ORDER OF EXECUTION. One list per flag kind,
    run kind by kind, cannot express "open this, photograph it, close it, open
    the next", so every picture would cost its own boot of a 10 GB ISO — a
    README's worth of them is a dozen boots.
    """

    def __call__(self, parser, ns, value, option_string=None):
        ns.steps.append((self.dest, value))


def main():
    ap = argparse.ArgumentParser()
    ap.set_defaults(steps=[])
    ap.add_argument("--out", default="/tmp/kdos-shot.ppm",
                    help="where the FINAL shot goes when no --shot was asked "
                         "for; ignored once one is")
    ap.add_argument("--shot", action=Step,
                    help="photograph the screen now, into this path")
    ap.add_argument("--sleep", action=Step, type=float,
                    help="wait this many seconds before the next step")
    ap.add_argument("--keys", action=Step,
                    help="monitor sendkey, e.g. meta_l or ctrl-esc")
    ap.add_argument("--chord", action=Step,
                    help="hold the modifiers and tap the key, over RFB — "
                         "`meta+shift+t`, `ctrl+alt+f2`. Use this for "
                         "anything with two modifiers in it: qemu's sendkey "
                         "presses and releases the whole combination at once "
                         "and such a chord never arrives")
    ap.add_argument("--type", action=Step,
                    help="type this into whatever has the focus, then Return "
                         "— the greeter's password field, KRunner, a Konsole "
                         "the run has opened")
    ap.add_argument("--text", action=Step,
                    help="type this and stop — no Return. A search field, a "
                         "filter or a rename is answered as it is typed, and "
                         "a Return has already acted on the first match")
    ap.add_argument("--mouse", action=Step,
                    help="move the pointer to X,Y (absolute pixels)")
    ap.add_argument("--click", action=Step,
                    help="X,Y[,BTN] — move there and click; BTN 1/2/3")
    ap.add_argument("--sweep", action=Step,
                    help="X1,Y1,X2,Y2[,N[,MS[,BTN]]] — a drag delivered at a "
                         "real mouse's report rate: N interpolated motions MS "
                         "milliseconds apart with the button held. --drag and "
                         "--press space their events 150ms apart, so a defect "
                         "that only appears under a fast hand cannot be "
                         "reproduced with either. Defaults: N=120, MS=2, BTN=1")
    ap.add_argument("--press", action=Step,
                    help="X,Y[,BTN] — press there and HOLD, on a connection "
                         "that stays open. Every later --press moves the "
                         "pointer with the button still down, so a --shot "
                         "between two of them photographs the drag in "
                         "progress; --release ends it")
    ap.add_argument("--release", action=Step, nargs="?", const="",
                    help="let go, ending the held drag — at X,Y if one is "
                         "given, otherwise where the last --press left the "
                         "pointer")
    ap.add_argument("--drag", action=Step,
                    help="X1,Y1,X2,Y2[,BTN] — press at the first point, move "
                         "to the second with the button held, release. A "
                         "--click cannot express this: its three events share "
                         "one coordinate, so no motion ever arrives with the "
                         "button down, and a drag begins on motion")
    ap.add_argument("--cmd", action=Step,
                    help="run in the session: a login shell of --user with "
                         "the running plasmashell's environment, in the "
                         "background")
    ap.add_argument("--root-cmd", action=Step,
                    help="run as root on the serial console, wait for it to "
                         "finish and print what it said")
    ap.add_argument("--root-script", action=Step,
                    help="'FILE [ARGS...]' — send this LOCAL file into the "
                         "guest and run it as root with these arguments; the "
                         "form for a check too long to be one line. A script "
                         "that exits non-zero fails the run (exit 3) once "
                         "every step has run")
    ap.add_argument("--dismiss-picker", action=Step, nargs="?", const=30.0,
                    type=float, metavar="SECONDS",
                    help="close the first-run plasma-welcome window (the desk "
                         "picker and the tour) once it opens, waiting up to "
                         "SECONDS (30) for it. It opens over every live "
                         "session, so put this before the first --shot")
    ap.add_argument("--size", default="1280x800",
                    help="the guest's screen, WxH — the virtio-gpu default is "
                         "1280x800 and nothing in the guest overrides it")
    ap.add_argument("--gl", action="store_true",
                    help="virtio-vga-gl on an egl-headless display, so KWin "
                         "composites with OpenGL on the host GPU and Phosphor "
                         "mode's kdos_crt can load. Needs /dev/dri; without "
                         "it KWin is on llvmpipe and the effect stays out")
    ap.add_argument("--venus", action="store_true",
                    help="with --gl, publish a Vulkan capset (venus + blob "
                         "resources over a memfd), the only way a guest Vulkan "
                         "tool can reach a host GPU. Measured on an NVIDIA "
                         "host it works at neither end: QEMU aborts with the "
                         "container toolkit attached, and the virtio ICD kills "
                         "vkCreateInstance without it. Without --venus the "
                         "tools measure lavapipe on the CPU and say nothing, "
                         "so check vulkaninfo before trusting a number")
    ap.add_argument("--until", choices=("plasma", "greeter", "shell"),
                    default="plasma",
                    help="what the readiness gate waits for before the first "
                         "step: plasma (the default) is the autologin "
                         "session with its shell up and KSplash done; "
                         "greeter is SDDM's greeter, for a disk with no "
                         "autologin; shell is the serial root shell alone, "
                         "for a run whose root script is the test")
    ap.add_argument("--no-session", dest="until", action="store_const",
                    const="shell",
                    help="the same as --until shell")
    ap.add_argument("--gate-timeout", type=int, default=300,
                    help="seconds from the serial shell to a passed gate "
                         "before the run fails (300)")
    ap.add_argument("--user", default="kdos",
                    help="the session's account, for the gate, --cmd and "
                         "--dismiss-picker (kdos)")
    ap.add_argument("--console-cmd", default=None,
                    help="switch to tty2 with Ctrl+Alt+F2 after the gate, log "
                         "in there as --console-login and type this, before "
                         "any step. The screen stays on tty2; `--chord "
                         "ctrl+alt+f1` returns to the session")
    ap.add_argument("--console-login", default="kdos:kdos",
                    help="USER[:PASSWORD] for --console-cmd's login on tty2; "
                         "the live medium's kdos password is kdos. With no "
                         "password nothing is typed at the prompt")
    ap.add_argument("--soak", type=int, default=0,
                    help="seconds to let the session run once, after the "
                         "last step and before the --out shot, for a load "
                         "test that has to last; --sleep is the same wait "
                         "placed by hand between steps")
    ap.add_argument("--wait", type=int, default=4,
                    help="seconds to let the screen settle once the gate has "
                         "passed: the splash fades and the panel finishes "
                         "laying out after the shell reports itself done")
    ap.add_argument("--script-timeout", type=int, default=900,
                    help="how long a --root-script or a --root-cmd may "
                         "take. Installing a "
                         "pack off the medium is minutes, not seconds")
    ap.add_argument("--vnc-port", type=int, default=5909)
    ap.add_argument("--audio", action="store_true",
                    help="give the guest an HDA controller, so audio paths "
                         "reach a real device: on the host's sound server "
                         "when testing/qemu-audio.sh finds one, a silent "
                         "backend when it does not")
    ap.add_argument("--disk", default=None,
                    help="attach this qcow2 as a virtio disk — the target an "
                         "unattended kinstall writes to. With the ISO attached "
                         "the run refuses when the qcow2 could grow past the "
                         "host's free space")
    ap.add_argument("--allow-disk-growth", action="store_true",
                    help="run an install onto --disk even when its qcow2 "
                         "could outgrow the host's free space")
    ap.add_argument("--no-cdrom", action="store_true",
                    help="leave the ISO off entirely, so the DISK is what "
                         "boots. The second half of an install test")
    ap.add_argument("--root-password", default="kdos",
                    help="what to answer when the serial console's login "
                         "asks root for a password, as an installed system "
                         "does. The default is the one install-to-disk.sh "
                         "sets")
    ap.add_argument("--boot-disk", action="store_true",
                    help="boot the DISK with the ISO still attached — an "
                         "installed system that can still reach the medium. "
                         "The app sweep reads packs from /mnt/iso/packs, "
                         "which only a medium you have put packs into "
                         "carries; the shipped ISO has none")
    ap.add_argument("--scratch", default=None,
                    help="attach a raw file as a virtio disk the GUEST writes "
                         "a tar onto, which is how files come back out. "
                         "--data-disk is input only")
    ap.add_argument("--usb", default=None,
                    help="attach a raw disk image as a removable USB 3 "
                         "stick on its own xHCI controller")
    ap.add_argument("--data-disk", default=None,
                    help="attach a raw file as a plain virtio disk — how a "
                         "large artefact, or a tar of files the guest "
                         "unpacks itself, reaches a guest with no network. "
                         "Not removable, unlike --usb. "
                         "testing/quick.sh carries its patch this way")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--serial-log", default="/tmp/kdos-serial.log")
    args = ap.parse_args()

    if not os.path.exists(ISO) and not args.no_cdrom:
        sys.exit("no ISO at %s — run make build first" % ISO)

    run = "/tmp/kdos-vnc-%d" % os.getpid()
    os.makedirs(run, exist_ok=True)
    ser_sock = run + "/serial.sock"
    mon_sock = run + "/monitor.sock"

    # The firmware, probed rather than hardcoded: Debian and Ubuntu have
    # shipped it under both spellings, and a host that has lost its `ovmf`
    # package leaves the DIRECTORY behind — so "the path exists" is not the
    # question, "the file is there" is. Failing here with the list beats
    # qemu's own "could not load PC BIOS", which does not say where it looked.
    ovmf = next((f for f in ("/usr/share/ovmf/OVMF.fd",
                             "/usr/share/OVMF/OVMF.fd",
                             "/usr/share/qemu/OVMF.fd")
                 if os.path.isfile(f)), None)
    if not ovmf:
        raise SystemExit("no OVMF.fd — looked in /usr/share/{ovmf,OVMF,qemu}")

    try:
        sw, sh = (int(v) for v in args.size.lower().split("x"))
    except ValueError:
        raise SystemExit("--size wants WxH, e.g. 1920x1080")

    # The screen is the DEVICE's, not the guest's: nothing in KDOS asks for a
    # mode, so virtio-gpu's own 1280x800 default is what KWin and the greeter
    # lay themselves out for unless xres/yres say otherwise.
    video = "virtio-vga-gl" if args.gl else "virtio-vga"
    video += ",xres=%d,yres=%d" % (sw, sh)
    display = "egl-headless" if args.gl else "none"

    # Vulkan reaches a GPU only through venus, and venus needs the device to
    # publish a Vulkan capset: without venus=true it publishes none, the guest's
    # virtio ICD opens nothing, and every Vulkan tool silently lands on lavapipe.
    # A number taken without --venus is a CPU rasteriser's, whatever it says.
    #
    # OPT-IN, and it has to stay opt-in, because BOTH ends of it are measured to
    # fail on an NVIDIA host: with the container toolkit attached (--gpus all,
    # the only way the container has a host Vulkan device to proxy to) QEMU
    # aborts during guest boot, exit 134; without it the guest keeps booting but
    # the virtio ICD now loads and takes instance creation down with it, so
    # vulkaninfo answers ERROR_OUT_OF_HOST_MEMORY and even lavapipe is gone.
    # Measuring lavapipe and saying so beats both, which is why the default
    # device must not change.
    mem = ["-m", "4G"]
    if args.venus:
        if not args.gl:
            raise SystemExit("--venus needs --gl: venus rides on virtio-vga-gl")
        print("--venus: check vulkaninfo names a GPU before trusting any "
              "number — a dead emulator and a dead ICD both look like a slow "
              "boot from here", flush=True)
        video += ",venus=true,blob=true,hostmem=4G"
        mem = ["-object", "memory-backend-memfd,id=mem,size=4G,share=on",
               "-machine", "memory-backend=mem", "-m", "4G"]

    qemu = [
        "qemu-system-x86_64", "-enable-kvm", "-cpu", "host",
        "-smp", str(min(8, os.cpu_count() or 4)), *mem,
        "-bios", ovmf,
        "-vga", "none", "-device", video,
        "-display", display,
        "-vnc", "127.0.0.1:%d" % (args.vnc_port - 5900),
        "-usb", "-device", "usb-tablet",
        "-chardev", "socket,id=ser0,path=%s,server=on,wait=off" % ser_sock,
        "-serial", "chardev:ser0",
        "-chardev", "socket,id=mon0,path=%s,server=on,wait=off" % mon_sock,
        "-monitor", "chardev:mon0",
        "-netdev", "user,id=net0", "-device", "virtio-net-pci,netdev=net0",
    ]
    if not args.no_cdrom:
        qemu += ["-cdrom", ISO]
    if args.disk:
        # AN INSTALL RUN FILLS THE QCOW2 ON THE HOST. The copy writes about
        # the payload `kinstall --dump probe` reports, and a host disk that
        # fills mid-copy pauses the emulator with the guest half-written, so
        # the room the image may still grow into is checked against what the
        # host has free before anything boots.
        if not args.no_cdrom and not args.boot_disk and \
                not args.allow_disk_growth:
            try:
                info = json.loads(subprocess.run(
                    ["qemu-img", "info", "-U", "--output=json", args.disk],
                    capture_output=True, text=True, check=True).stdout)
            except (OSError, subprocess.CalledProcessError, ValueError) as e:
                raise SystemExit("--disk %s: qemu-img info failed: %s"
                                 % (args.disk, e))
            grow = info["virtual-size"] - info.get("actual-size", 0)
            free = shutil.disk_usage(
                os.path.dirname(os.path.abspath(args.disk))).free
            if grow > free:
                raise SystemExit(
                    "--disk %s can grow by %d MB and the host has %d MB "
                    "free: make a smaller qcow2, free space, or pass "
                    "--allow-disk-growth" % (args.disk, grow >> 20,
                                             free >> 20))
        # virtio-blk, so it lands as /dev/vda and kinstall's probe sees a plain
        # whole disk rather than something behind a USB bridge.
        #
        # WHICHEVER IS ATTACHED IS WHAT BOOTS, and the order has to be said
        # rather than left to the firmware. With a CD present this run is the
        # INSTALL — boot the medium, or the second run tests the disk the first
        # one wrote and the first tests nothing. With no CD it is the run that
        # boots what was installed. Leaving the default worked only while the
        # disk was blank, which is the one case where getting it wrong is
        # invisible.
        qemu += ["-drive", "if=none,id=hd0,format=qcow2,file=%s" % args.disk,
                 "-device", "virtio-blk-pci,drive=hd0",
                 "-boot", "order=c" if (args.no_cdrom or args.boot_disk)
                          else "order=d"]
    if args.audio:
        # AN HDA CONTROLLER, AND THE SAMPLES GO SOMEWHERE REAL WHERE THEY CAN.
        # `-audiodev none` is a real backend as far as the guest is concerned:
        # the kernel binds snd_hda_intel, ALSA opens the PCM, and anything that
        # asks "is there a sound card" gets yes. Without it PipeWire has no
        # sink, Plasma's volume applet shows none, and every audio path in the
        # guest is untestable — which is not the same as untested.
        #
        # A null sink consumes samples on a timer, though, and a host one
        # consumes them as a device does: a guest whose pacing follows its own
        # playback position runs to a different clock on the two, so the same
        # probe every `make run*` uses is asked first and `none` is the answer
        # when it finds nothing. The container needs the driver AND the
        # socket — see testing/qemu-audio.sh.
        probe = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "qemu-audio.sh")
        real = ""
        try:
            real = subprocess.run(["bash", probe], capture_output=True,
                                  text=True, timeout=20).stdout.strip()
        except Exception:
            real = ""
        if real:
            qemu += real.split()
        else:
            qemu += ["-audiodev", "none,id=snd0", "-device", "intel-hda",
                     "-device", "hda-output,audiodev=snd0"]
    if args.scratch:
        # A RAW DISK THE GUEST WRITES A TAR ONTO, and the only path OUT of a
        # guest with no network. `--data-disk` carries files IN; nothing
        # carried them back, and a hundred screenshots do not fit through a
        # serial console. The guest runs `tar cf /dev/vdX <dir>` — no
        # filesystem and no mkfs, because tar on a block device starts at
        # offset 0 and the host reads it with a plain `tar xf`, which stops at
        # the archive's own end marker and never looks at the trailing
        # megabytes.
        qemu += ["-drive", "if=none,id=scr0,format=raw,file=%s" % args.scratch,
                 "-device", "virtio-blk-pci,drive=scr0"]
    if args.data_disk:
        # A PLAIN VIRTIO DISK CARRYING ONE FILE'S BYTES, for getting a large
        # artefact INTO a guest that has no network. It is the same
        # virtio-blk the target disk uses and is not removable, so the
        # session's removable-media handling has no stick to act on. It lands
        # as the next /dev/vdX, has no partition table and no filesystem —
        # the guest reads the device itself, with `dd bs=1M` and
        # a `truncate` to the artefact's exact length, because a raw drive is
        # rounded up to a sector boundary.
        qemu += ["-drive", "if=none,id=dd0,format=raw,file=%s" % args.data_disk,
                 "-device", "virtio-blk-pci,drive=dd0"]
    if args.usb:
        # A real removable device, for udisks2, Solid and the Device Notifier:
        # usb-storage lands as /dev/sdX with `removable = 1`, which is what
        # they treat as a stick rather than as a fixed disk. It goes on its
        # own xHCI controller: `-usb` on this machine type is UHCI, USB 1.1,
        # and a session booted off a stick on it reads its image so slowly
        # that the login outruns turnstile's login_timeout and Plasma never
        # starts.
        qemu += ["-device", "qemu-xhci,id=xhci",
                 "-drive", "if=none,id=stick,format=raw,file=%s" % args.usb,
                 "-device",
                 "usb-storage,bus=xhci.0,drive=stick,removable=on"]
    print("+", " ".join(qemu), flush=True)
    vm = subprocess.Popen(qemu, stdout=subprocess.DEVNULL,
                          stderr=subprocess.STDOUT)

    failed = []
    try:
        if not wait_for(ser_sock) or not wait_for(mon_sock):
            raise SystemExit("qemu did not create its sockets")
        ser = Serial(ser_sock)
        mon = Monitor(mon_sock)

        # The serial console is a ROOT login on this image — and depending on
        # how far the boot has got it is either a `login:` prompt or a shell
        # that is already there. Probing for a shell rather than expecting a
        # prompt covers both, and covers the third case nobody thinks about:
        # a prompt whose text is a starship theme with escape codes in it.
        #
        # NOT A KEYSTROKE UNTIL THE KERNEL HAS THE PORT. Limine reads the
        # serial console as INPUT, and any byte arriving during its countdown
        # cancels the countdown and leaves the menu up — indefinitely, because
        # nothing is going to press Enter. A probe loop that starts typing the
        # moment it connects gets about eight keystrokes into a ten-second
        # window, so the rig reliably stops the boot it is waiting for and then
        # reports `no shell on the serial console`, which reads as a broken
        # image rather than as the harness.
        #
        # Measured on one ISO, same qemu, same socket: receive-only reaches a
        # root shell with 9259 bytes; probing from connect stops at 328 bytes
        # and never leaves the firmware.
        #
        # So the boot is WATCHED first and typed at only once something past the
        # bootloader has spoken.
        #
        # `[KDOS]` IS THE MARKER, AND THE KERNEL'S OWN BANNER IS NOT. The Live
        # cmdline carries `quiet loglevel=3`, so the kernel prints essentially
        # nothing to ttyS0 — waiting for `Linux version` waits for a line this
        # image never writes, and times out on a boot that is going perfectly.
        # `[KDOS]` is rcS announcing each service, which is userspace running
        # and therefore the port long since handed over. `login:` and a root
        # prompt are kept as alternates for an image that reaches one first.
        #
        print("waiting for the kernel to take the serial console…", flush=True)
        boot_deadline = time.time() + 180
        while time.time() < boot_deadline:
            ser.pump()
            if b"[KDOS]" in ser.buf or b"login:" in ser.buf \
               or b"root@" in ser.buf or b"Linux version" in ser.buf:
                break
            time.sleep(0.25)
        else:
            print(ser.tail(4000))
            raise SystemExit(
                "the bootloader never handed over — if the last thing on the "
                "console is the firmware, something typed at the menu")

        print("waiting for a shell on the serial console…", flush=True)
        deadline = time.time() + 240
        ready = False
        tries = 0
        while time.time() < deadline and not ready:
            ser.pump()
            # THE LIVE IMAGE LOGS ROOT IN WITH NO PASSWORD; AN INSTALLED ONE
            # ASKS. The answer goes only straight after `root`, never on its
            # own: on a line that is already a shell it would run as a
            # command.
            if b"login:" in ser.buf[-400:]:
                ser.send("root")
                time.sleep(2)
                ser.pump()
                if b"assword" in ser.buf[-200:]:
                    ser.send(args.root_password)
                    time.sleep(3)
                    ser.pump()
            ser.send("echo KDOS''PROBE")
            tries += 1
            if ser.expect("KDOSPROBE", timeout=4):
                ready = True
        if not ready:
            print(ser.tail(4000))
            raise SystemExit("no shell on the serial console")
        ser.send("stty -echo 2>/dev/null; PS1='KDOSREADY# '")
        time.sleep(1)
        ser.pump()

        # THE READINESS GATE. The serial shell arrives with SDDM, not after
        # the session, so a fixed sleep here either wastes a minute or
        # photographs a splash — and when the session never comes up it
        # photographs a black screen and calls that the result. The gate asks
        # the guest what is running instead, and a run that does not pass it
        # stops here with the reason. GATE_SH says what each answer means.
        if args.until != "shell":
            what = "Plasma session" if args.until == "plasma" else "SDDM greeter"
            print("waiting for the %s (up to %ds)…" % (what, args.gate_timeout),
                  flush=True)
            out, rc = guest_run(ser, GATE_SH,
                                [args.user, args.until, str(args.gate_timeout)],
                                name="gate", timeout=args.gate_timeout + 60)
            print(out, flush=True)
            if rc != 0:
                raise SystemExit("[rig] the readiness gate failed: no %s"
                                 % what)
            time.sleep(args.wait)

        # The session account's environment, for every --cmd; ENV_SH says
        # where it comes from. Written once, readable by the account.
        guest_run(ser, "mkdir -p /run/kdos-rig\nchmod 0755 /run/kdos-rig\n"
                       "cat > /run/kdos-rig/env.sh <<'KDOS_RIG_ENV_FILE'\n%s\n"
                       "KDOS_RIG_ENV_FILE\nchmod 0644 /run/kdos-rig/env.sh\n"
                       % ENV_SH.strip("\n"),
                  name="env", timeout=30)

        if args.console_cmd:
            # tty1 IS KWIN'S, so a text console is tty2: Ctrl+Alt+F2 asks
            # KWin to have logind switch to it, the getty there asks for a
            # login, and the command goes in once the shell has had time to
            # read the profile. The screen stays on tty2 for the steps.
            login, _, password = args.console_login.partition(":")
            print("logging in on tty2 as %s: %s" % (login, args.console_cmd),
                  flush=True)
            rfb_chord("127.0.0.1", args.vnc_port, "ctrl+alt+f2")
            time.sleep(2)
            mon.type(login)
            time.sleep(2)
            if password:
                mon.type(password)
            time.sleep(3)
            mon.type(args.console_cmd)
            time.sleep(2)

        shots = 0
        for kind, value in args.steps:
            if kind == "cmd":
                # A LOGIN shell: a plain `su kdos -c` leaves HOME unset for
                # podman and every appbox call fails. env.sh then gives it the
                # session's display, bus and config directories.
                ser.send("su - %s -c '. /run/kdos-rig/env.sh; %s' &"
                         % (args.user, value))
                time.sleep(4)
            elif kind == "keys":
                mon.cmd("sendkey " + value)
                time.sleep(3)
            elif kind == "chord":
                rfb_chord("127.0.0.1", args.vnc_port, value)
                time.sleep(3)
            elif kind == "type":
                # TYPED AS A STEP, so it reaches whatever has the focus AT
                # THIS POINT of the run — a window the run has just opened,
                # the greeter's password field, KRunner.
                mon.type(value)
                time.sleep(2)
            elif kind == "text":
                mon.type(value, ret=False)
                time.sleep(2)
            elif kind == "mouse":
                mx, my = (int(v) for v in value.split(","))
                rfb_pointer("127.0.0.1", args.vnc_port, [(mx, my, 0)])
                # Longer than a tooltip's dwell, so a shot after a move
                # photographs the tooltip as well as the hover.
                time.sleep(2)
            elif kind == "click":
                parts = value.split(",")
                mx, my = int(parts[0]), int(parts[1])
                btn = int(parts[2]) if len(parts) > 2 else 1
                mask = 1 << (btn - 1)
                rfb_pointer("127.0.0.1", args.vnc_port,
                            [(mx, my, 0), (mx, my, mask), (mx, my, 0)])
                time.sleep(2.5)
            elif kind == "sweep":
                p = value.split(",")
                x0, y0, x1, y1 = (int(v) for v in p[:4])
                n = int(p[4]) if len(p) > 4 else 120
                ms = int(p[5]) if len(p) > 5 else 2
                btn = int(p[6]) if len(p) > 6 else 1
                rfb_sweep("127.0.0.1", args.vnc_port, x0, y0, x1, y1,
                          n, 1 << (btn - 1), ms / 1000.0)
                time.sleep(2.5)
            elif kind == "press":
                parts = value.split(",")
                mx, my = int(parts[0]), int(parts[1])
                btn = int(parts[2]) if len(parts) > 2 else 1
                mask = 1 << (btn - 1)
                if _held["sock"] is None:
                    # THE ENTER BEFORE THE PRESS. A press at a point the
                    # pointer was never at is a press the session routes
                    # against whatever it last thought was under the cursor.
                    rfb_hold("127.0.0.1", args.vnc_port, mx, my, 0)
                    _held["last"] = (mx, my)
                rfb_hold("127.0.0.1", args.vnc_port, mx, my, mask)
                _held["last"] = (mx, my)
                time.sleep(0.6)
            elif kind == "release":
                if value:
                    mx, my = (int(v) for v in value.split(","))
                else:
                    mx, my = _held.get("last", (0, 0))
                rfb_drop("127.0.0.1", args.vnc_port, mx, my)
                time.sleep(2.0)
            elif kind == "drag":
                # TWELVE INTERPOLATED POINTS AND NOT ONE. A drag is a press,
                # then MOTION with the button held, then a release, and each
                # RFB event is its own tick. A jump straight to the far point
                # is a press and a release at two places with nothing in
                # between, which KWin and Qt both read as a click.
                p = value.split(",")
                x0, y0, x1, y1 = (int(v) for v in p[:4])
                mask = 1 << ((int(p[4]) if len(p) > 4 else 1) - 1)
                mv = [(x0, y0, 0), (x0, y0, mask)]
                for i in range(1, 13):
                    mv.append((x0 + (x1 - x0) * i // 12,
                               y0 + (y1 - y0) * i // 12, mask))
                mv.append((x1, y1, 0))
                rfb_pointer("127.0.0.1", args.vnc_port, mv)
                time.sleep(2.5)
            elif kind == "sleep":
                # Pumped, not slept through: the serial console fills and
                # blocks the guest while nothing is reading it.
                end = time.time() + value
                while time.time() < end:
                    time.sleep(min(2.0, max(0.1, end - time.time())))
                    ser.pump()
                    ser.buf = ser.buf[-8000:]
            elif kind == "dismiss_picker":
                out, _rc = guest_run(ser, DISMISS_SH,
                                     [args.user, str(int(value))],
                                     name="dismiss", timeout=int(value) + 60)
                print(out, flush=True)
                time.sleep(1.5)
            elif kind == "root_cmd":
                # THE MARKERS ARE TYPED WITH AN EMPTY QUOTE INSIDE THEM, so the
                # terminal's echo of the typed line never holds what the guest
                # prints: the wait ends when the command does, not when its own
                # echo arrives. A marker with `<` in it is a redirection to the
                # shell, which prints no marker at all.
                ser.buf = b""
                ser.send("echo ===KDOSCMD-''BEGIN===; %s 2>&1; "
                         "echo ===KDOSCMD-''END===" % value)
                done = ser.expect("===KDOSCMD-END===",
                                  timeout=args.script_timeout)
                ser.pump()
                out = ser.buf.decode("utf-8", "replace")
                cut = out.rfind("===KDOSCMD-BEGIN===")
                if cut >= 0:
                    out = out[cut + len("===KDOSCMD-BEGIN==="):]
                end = out.find("===KDOSCMD-END===")
                if end >= 0:
                    out = out[:end]
                if not done:
                    out += "\n[rig] the command never finished"
                print("$ %s\n%s" % (value, out[-60000:]), flush=True)
            elif kind == "root_script":
                argv = shlex.split(value)
                path, rest = argv[0], argv[1:]
                body = open(path, "rb").read()
                name = re.sub(r"[^A-Za-z0-9_.-]", "_",
                              os.path.basename(path)) or "step"
                out, rc = guest_run(ser, body, rest, name=name,
                                    timeout=args.script_timeout)
                print("$ sh %s\n%s" % (value, out), flush=True)
                if rc is None or rc != 0:
                    print("[rig] %s exited %s" % (path, rc), flush=True)
                    failed.append("%s (%s)" % (path, rc))
            elif kind == "shot":
                w, h = rfb_shot("127.0.0.1", args.vnc_port, value)
                print("wrote %s (%dx%d)" % (value, w, h), flush=True)
                shots += 1

        # A soak is the only way to ask "what does this cost over time".
        # A monitor's own CPU share is meaningless over four seconds: the
        # first sample is startup, and startup is exactly what is not being
        # measured. The serial is pumped so the console does not fill and
        # block the guest while nothing is reading it.
        if args.soak:
            print("soaking for %ds…" % args.soak, flush=True)
            end = time.time() + args.soak
            while time.time() < end:
                time.sleep(5)
                ser.pump()
                ser.buf = ser.buf[-8000:]

        # `--out` is the one-shot form and stays the default: a run that asked
        # for no picture at all is a run that booted the ISO for nothing, and
        # what is on the screen at the end of it is the desktop the boot
        # brought up.
        if not shots:
            time.sleep(2)
            print("reading the framebuffer over VNC…", flush=True)
            w, h = rfb_shot("127.0.0.1", args.vnc_port, args.out)
            print("wrote %s (%dx%d)" % (args.out, w, h))

        ser.send("pgrep -a 'sddm|kwin_wayland|plasmashell|kdos-' | head -20")
        time.sleep(1)
        ser.pump()
        with open(args.serial_log, "w") as f:
            f.write(ser.tail(20000))
        print("serial tail in", args.serial_log)
    finally:
        if not args.keep:
            vm.terminate()
            try:
                vm.wait(timeout=10)
            except subprocess.TimeoutExpired:
                vm.kill()
        else:
            print("VM left running (pid %d); sockets in %s" % (vm.pid, run))
    # A FAILED ROOT SCRIPT FAILS THE RUN, after every step has run and the VM
    # is down: a probe's verdict is the run's exit status, so `make` and a
    # loop can act on it, and the photographs taken around it are still
    # written.
    if failed:
        print("[rig] root scripts that failed: " + ", ".join(failed),
              flush=True)
        sys.exit(3)


if __name__ == "__main__":
    main()

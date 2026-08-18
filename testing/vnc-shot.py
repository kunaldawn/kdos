#!/usr/bin/env python3
"""Boot the ISO headless, drive it over serial, and photograph the screen.

WHY THIS EXISTS. Six geometry defects have shipped in this toolkit and not one
was visible to a compiler; `testing/goldens` catches the ones a dump can see,
and everything a dump CANNOT see — the compositor, the wallpaper under the
desktop, the icon layer, a menu anchored to the wrong corner — is only visible
by looking at the screen. This is how the screen gets looked at without a
human in front of it.

    testing/vnc-shot.py                       boot, wait for the desktop, shoot
    testing/vnc-shot.py --keys meta_l-a       press something first
    testing/vnc-shot.py --cmd 'kdos-net &'    run something in the session first
    testing/vnc-shot.py --keep                leave the VM up afterwards

Three things it has to get right, each recorded in CLAUDE.md's VM debug rig
section before this file existed:

  - `screendump` over the monitor socket answers "no surface" under a GL
    display, so the framebuffer is read over RFB instead. The handshake's
    SetEncodings message is `type(1) pad(1) count(2)` followed by count
    int32s — an extra padding field desyncs the stream and every later read
    blocks forever.
  - The serial console is a ROOT login on this image, and driving the session
    means `su - kdos -c` — a LOGIN shell, because a plain `su kdos -c` leaves
    podman resolving HOME to `/` and every call fails.
  - `-display none -vnc` with plain virtio-vga puts wlroots on the pixman
    renderer, so the CRT pass declines and the phosphor shader is NOT in the
    picture. That is a limitation of the rig, not of the desktop, and the
    shot is of the cell grid underneath it.
"""

import argparse
import os
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

    def send(self, line):
        self.s.sendall(line.encode() + b"\n")
        time.sleep(0.3)

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

    def type(self, text):
        """Type on the VT, one `sendkey` per character.

        The session is started the way a person starts it — by typing
        `kdos-desktop` at the autologin prompt on tty1 — rather than from the
        serial root shell, and that is not a stylistic choice: wlroots' DRM
        backend needs a seat, and seatd grants one to a session that is ON a
        VT. A compositor launched from ttyS0 gets no seat and never opens the
        display.
        """
        names = {"-": "minus", ".": "dot", "/": "slash", " ": "spc",
                 "_": "shift-minus", "=": "equal"}
        for ch in text:
            self.cmd("sendkey " + names.get(ch, ch))
            time.sleep(0.08)
        self.cmd("sendkey ret")


def rfb_shot(host, port, out):
    """One framebuffer, over RFB, Raw encoding, written as a PPM.

    No compression is negotiated on purpose: a raw 1280x800 frame is 4 MB over
    a loopback socket, and a decoder for tight/zrle here would be more code
    than the thing it is testing.
    """
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

    bpp, depth, big_endian, true_colour = pf[0], pf[1], pf[2], pf[3]
    rmax, gmax, bmax = struct.unpack(">HHH", pf[4:10])
    rsh, gsh, bsh = pf[10], pf[11], pf[12]
    if bpp not in (8, 16, 32) or not true_colour:
        raise RuntimeError("unhandled pixel format bpp=%d true=%d"
                           % (bpp, true_colour))

    # SetEncodings: type(1) pad(1) count(2), then count int32s. The extra pad
    # everybody adds here is what desyncs the stream.
    s.sendall(struct.pack(">BBH", 2, 0, 1) + struct.pack(">i", 0))
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/kdos-shot.ppm")
    ap.add_argument("--keys", action="append", default=[],
                    help="monitor sendkey, e.g. meta_l-a")
    ap.add_argument("--cmd", action="append", default=[],
                    help="run in the kdos session before the shot")
    ap.add_argument("--root-cmd", action="append", default=[],
                    help="run as root on the serial console and print it")
    ap.add_argument("--wait", type=int, default=25,
                    help="seconds to let the session settle")
    ap.add_argument("--vnc-port", type=int, default=5909)
    ap.add_argument("--usb", default=None,
                    help="attach a raw disk image as a USB stick")
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--serial-log", default="/tmp/kdos-serial.log")
    args = ap.parse_args()

    if not os.path.exists(ISO):
        sys.exit("no ISO at %s — run make build first" % ISO)

    run = "/tmp/kdos-vnc-%d" % os.getpid()
    os.makedirs(run, exist_ok=True)
    ser_sock = run + "/serial.sock"
    mon_sock = run + "/monitor.sock"

    qemu = [
        "qemu-system-x86_64", "-enable-kvm", "-cpu", "host",
        "-smp", str(min(8, os.cpu_count() or 4)), "-m", "4G",
        "-bios", "/usr/share/ovmf/OVMF.fd",
        "-cdrom", ISO,
        "-vga", "none", "-device", "virtio-vga",
        "-display", "none",
        "-vnc", "127.0.0.1:%d" % (args.vnc_port - 5900),
        "-usb", "-device", "usb-tablet",
        "-chardev", "socket,id=ser0,path=%s,server=on,wait=off" % ser_sock,
        "-serial", "chardev:ser0",
        "-chardev", "socket,id=mon0,path=%s,server=on,wait=off" % mon_sock,
        "-monitor", "chardev:mon0",
        "-netdev", "user,id=net0", "-device", "virtio-net-pci,netdev=net0",
    ]
    if args.usb:
        # A real removable device, because kdos-mountd's whole job is one and
        # a fixture cannot mount anything. usb-storage lands as /dev/sdX with
        # `removable = 1`, which is exactly what the daemon looks for.
        qemu += ["-drive", "if=none,id=stick,format=raw,file=%s" % args.usb,
                 "-device", "usb-storage,drive=stick,removable=on"]
    print("+", " ".join(qemu), flush=True)
    vm = subprocess.Popen(qemu, stdout=subprocess.DEVNULL,
                          stderr=subprocess.STDOUT)

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
        print("waiting for a shell on the serial console…", flush=True)
        deadline = time.time() + 240
        ready = False
        tries = 0
        while time.time() < deadline and not ready:
            ser.pump()
            if b"login:" in ser.buf[-400:]:
                ser.send("root")
                time.sleep(2)
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

        # tty1 autologins as `kdos` and stops at a prompt: the session is
        # started by hand on this distro, which is the documented entry point
        # (`kdos-desktop` from a tty) and not something to work around.
        print("starting the session on tty1…", flush=True)
        mon.type("kdos-desktop")

        print("waiting for the desktop…", flush=True)
        deadline = time.time() + 240
        up = False
        while time.time() < deadline and not up:
            ser.send("pgrep -x kdos-comp >/dev/null && echo COMPUP || echo NOCOMP")
            ser.expect("COMPUP", timeout=5)
            up = b"COMPUP" in ser.buf
        if not up:
            print(ser.tail(4000))
            raise SystemExit("kdos-comp never came up")
        time.sleep(args.wait)

        for c in args.cmd:
            # A LOGIN shell: a plain `su kdos -c` leaves HOME unset for podman
            # and every appbox call fails.
            ser.send("su - kdos -c 'export WAYLAND_DISPLAY=wayland-0; "
                     "export XDG_RUNTIME_DIR=/run/user/1000; %s' &" % c)
            time.sleep(4)
        for k in args.keys:
            mon.cmd("sendkey " + k)
            time.sleep(3)

        for c in args.root_cmd:
            ser.buf = b""
            ser.send("echo ---8<---; %s 2>&1; echo ---8<---" % c)
            ser.expect("---8<---", timeout=20)
            time.sleep(2)
            ser.pump()
            print("$ %s\n%s" % (c, ser.tail(8000)), flush=True)

        time.sleep(2)
        print("reading the framebuffer over VNC…", flush=True)
        w, h = rfb_shot("127.0.0.1", args.vnc_port, args.out)
        print("wrote %s (%dx%d)" % (args.out, w, h))

        ser.send("pgrep -a 'kdos-' | head -20")
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


if __name__ == "__main__":
    main()

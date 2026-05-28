#!/usr/bin/env python3
"""Drive the containerized HW-accel QEMU over a serial socket to prove the
noctalia (Qt) shell renders on the NVIDIA GPU via virgl+blob instead of blanking.

Boots gtk,gl=on + virtio-vga-gl,blob=true; logs in on ttyS0 (askfirst -> Enter);
silences tty echo; swaps /usr/bin/noctalia for a HW variant (no software force,
QSG_INFO=1); starts niri on a real VT via openvt (VT-bound seatd needs it);
dumps niri + noctalia logs and a screenshot, transferred back over serial as hex.
Window appears on the host display.  Pass --venus to also enable venus=true.
"""
import socket, subprocess, sys, time, os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PORT = 4555
CNAME = "kdos-hw-verify"
IMAGE = "kdos-qemu-hw"
VENUS = "--venus" in sys.argv
S, E = "@@S@@", "@@E@@"

def sh(*a):
    return subprocess.run(a, capture_output=True, text=True)

def start_container():
    sh("docker", "rm", "-f", CNAME)
    subprocess.run(["bash", "-c", "xhost +local:root >/dev/null 2>&1"], check=False)
    gpu = "blob=true,hostmem=4G,xres=1920,yres=1080"
    if VENUS:
        gpu = "blob=true,venus=true,hostmem=4G,xres=1920,yres=1080"
    qemu = (
        "qemu-system-x86_64 -enable-kvm -cpu host "
        "-object memory-backend-memfd,id=mem,size=4G,share=on "
        "-machine pc,memory-backend=mem,accel=kvm -m 4G "
        "-bios /usr/share/ovmf/OVMF.fd "
        f"-vga none -device virtio-vga-gl,{gpu} -display gtk,gl=on "
        "-cdrom /work/build/iso-build/kdos.iso "
        "-drive file=/work/build/kdos.qcow2,format=qcow2 "
        f"-chardev socket,id=s0,host=127.0.0.1,port={PORT},server=on,wait=off,telnet=off "
        "-serial chardev:s0 "
        "-netdev user,id=net0 -device virtio-net-pci,netdev=net0"
    )
    disp = os.environ.get("DISPLAY", ":1")
    r = sh("docker", "run", "-d", "--rm", "--name", CNAME, "--network", "host",
           "--gpus", "all",
           "--device", "/dev/kvm", "--device", "/dev/dri", "--device", "/dev/udmabuf",
           "-e", f"DISPLAY={disp}",
           "-v", "/tmp/.X11-unix:/tmp/.X11-unix:rw",
           "-v", "/usr/share/ovmf:/usr/share/ovmf:ro",
           "-v", f"{REPO}/build:/work/build",
           IMAGE, "-c", qemu)
    if r.returncode != 0:
        print("docker run failed:\n", r.stderr); sys.exit(1)
    print("[*] container:", r.stdout.strip()[:12], "venus=" + str(VENUS))

class Serial:
    def __init__(self):
        for _ in range(60):
            try:
                self.s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
                self.s.settimeout(1.0); return
            except OSError:
                time.sleep(1)
        print("[!] no serial connection"); sys.exit(1)
    def read_until(self, marker, timeout=60):
        buf = ""; end = time.time() + timeout
        while time.time() < end:
            try:
                d = self.s.recv(4096).decode("utf-8", "replace")
                if d: buf += d
                if marker in buf: return buf
            except socket.timeout:
                continue
        return buf
    def send(self, line):
        self.s.sendall((line + "\n").encode())
    def cap(self, cmd, timeout=40):
        # echo is off in the tty, so output between S and E is clean.
        self.send(f"printf '{S}\\n'; {cmd}; printf '{E}%d\\n' $?")
        out = self.read_until(E, timeout)
        if S in out and E in out:
            return out.split(S, 1)[1].rsplit(E, 1)[0].strip()
        return out.strip()

def main():
    start_container()
    ser = Serial()
    print("[*] waiting for boot")
    ser.read_until("Please press Enter", timeout=120)
    ser.send(""); time.sleep(1); ser.send(""); time.sleep(1)
    # Silence echo + bracketed paste so captures are clean.
    ser.send("stty -echo 2>/dev/null; bind 'set enable-bracketed-paste off' 2>/dev/null; PS1=; export PS1")
    time.sleep(1)
    print("[*] whoami:", ser.cap("id -un", 15))
    print("[*] runtime dir:", ser.cap("echo $XDG_RUNTIME_DIR; ls -ld $XDG_RUNTIME_DIR 2>&1", 15))
    print("[*] dri devices:", ser.cap("ls -l /dev/dri 2>&1", 15))

    # Test the SHIPPED wrapper + niri config as-is (no override).
    print("[*] SHIPPED wrapper:\n" + ser.cap("cat /usr/bin/noctalia", 15))
    print("[*] SHIPPED niri config output block:\n" + ser.cap("grep -nA3 'output' /etc/niri/config.kdl 2>&1 | head", 15))
    print("[*] gating decision (blob present?):",
          ser.cap("dmesg 2>/dev/null | grep -o '+resource_blob' | head -1; "
                  "if dmesg 2>/dev/null | grep -q +resource_blob; then echo MODE=HW; else echo MODE=SOFTWARE; fi", 15))

    # Launch niri on a real VT (VT-bound seatd) via openvt. Export QSG_INFO so the
    # (unmodified) shipped wrapper's noctalia logs its GL renderer to niri.log.
    print("[*] launching niri on a VT via openvt")
    ser.cap("rm -f /tmp/niri.log /tmp/noctalia.log; export QSG_INFO=1; "
            "setsid openvt -s -- /bin/sh -c 'exec niri-session >/tmp/niri.log 2>&1' </dev/null >/dev/null 2>&1 & "
            "sleep 1; echo launched", 20)
    time.sleep(20)

    print("[*] NIRI socket:", ser.cap("ls -1 ${XDG_RUNTIME_DIR:-/run/user/0}/niri*.sock 2>&1 | head -1", 15))
    print("\n===== niri.log (full, head 60) =====")
    print(ser.cap("cat /tmp/niri.log 2>&1 | head -60", 25))
    print("\n===== niri msg version (is niri up?) =====")
    print(ser.cap("export NIRI_SOCKET=$(ls -1 ${XDG_RUNTIME_DIR:-/run/user/0}/niri*.sock 2>/dev/null|head -1); niri msg version 2>&1", 20))
    print("\n===== niri msg outputs (resolution / modes) =====")
    print(ser.cap("export NIRI_SOCKET=$(ls -1 ${XDG_RUNTIME_DIR:-/run/user/0}/niri*.sock 2>/dev/null|head -1); niri msg outputs 2>&1 | head -25", 20))
    print("\n===== noctalia.log (QSG_INFO / errors) =====")
    print(ser.cap("cat /tmp/noctalia.log 2>&1 | head -60", 25))
    print("\n===== processes =====")
    print(ser.cap("ps 2>/dev/null | grep -iE 'niri|qs|quickshell' | grep -v grep", 15))
    print("\n===== niri layers =====")
    print(ser.cap("export NIRI_SOCKET=$(ls -1 ${XDG_RUNTIME_DIR:-/run/user/0}/niri*.sock 2>/dev/null|head -1); niri msg layers 2>&1 | head -40", 20))

    # Screenshot.
    print("\n[*] screenshot")
    print(ser.cap("export NIRI_SOCKET=$(ls -1 ${XDG_RUNTIME_DIR:-/run/user/0}/niri*.sock 2>/dev/null|head -1); "
                  "niri msg action screenshot-screen 2>&1; sleep 2; "
                  "ls -1t /root/Pictures/Screenshots/*.png 2>&1 | head -1", 25))
    print(ser.cap("SHOT=$(ls -1t /root/Pictures/Screenshots/*.png 2>/dev/null|head -1); "
                  "convert \"$SHOT\" -resize 1280 -quality 82 /tmp/shot.jpg 2>&1; ls -l /tmp/shot.jpg 2>&1", 30))
    out = ser.cap("od -An -v -tx1 /tmp/shot.jpg 2>/dev/null | tr -d ' \\n'", 150)
    hexs = "".join(ch for ch in out if ch in "0123456789abcdef")
    if len(hexs) > 2000 and len(hexs) % 2 == 0:
        with open("/tmp/kdos-hw-shot.jpg", "wb") as f:
            f.write(bytes.fromhex(hexs))
        print(f"[*] screenshot saved: /tmp/kdos-hw-shot.jpg ({len(hexs)//2} bytes)")
    else:
        print("[!] screenshot transfer failed; hex len=", len(hexs))

    sh("docker", "rm", "-f", CNAME)
    print("[*] done")

if __name__ == "__main__":
    main()

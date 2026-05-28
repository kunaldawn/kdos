#!/usr/bin/env python3
"""One-shot probe: boot the blob container, dump signals that distinguish a
blob-capable QEMU (HW dmabuf works) from plain virgl (needs software force),
so the noctalia wrapper can auto-gate. Tears down after."""
import socket, subprocess, sys, time, os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PORT = 4556
CNAME = "kdos-hw-probe"
IMAGE = "kdos-qemu-hw"
S, E = "@@S@@", "@@E@@"

def sh(*a): return subprocess.run(a, capture_output=True, text=True)

sh("docker", "rm", "-f", CNAME)
qemu = ("qemu-system-x86_64 -enable-kvm -cpu host "
        "-object memory-backend-memfd,id=mem,size=4G,share=on "
        "-machine pc,memory-backend=mem,accel=kvm -m 4G "
        "-bios /usr/share/ovmf/OVMF.fd -vga none "
        "-device virtio-vga-gl,blob=true,hostmem=4G -display egl-headless "
        "-cdrom /work/build/iso-build/kdos.iso "
        "-drive file=/work/build/kdos.qcow2,format=qcow2 "
        f"-chardev socket,id=s0,host=127.0.0.1,port={PORT},server=on,wait=off,telnet=off "
        "-serial chardev:s0 -netdev user,id=net0 -device virtio-net-pci,netdev=net0")
r = sh("docker", "run", "-d", "--rm", "--name", CNAME, "--network", "host", "--gpus", "all",
       "--device", "/dev/kvm", "--device", "/dev/dri", "--device", "/dev/udmabuf",
       "-v", "/usr/share/ovmf:/usr/share/ovmf:ro", "-v", f"{REPO}/build:/work/build",
       IMAGE, "-c", qemu)
if r.returncode != 0:
    print("docker run failed:", r.stderr); sys.exit(1)
print("[*] container:", r.stdout.strip()[:12])

s = None
for _ in range(60):
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2); s.settimeout(1.0); break
    except OSError: time.sleep(1)

def read_until(marker, timeout=60):
    buf = ""; end = time.time() + timeout
    while time.time() < end:
        try:
            d = s.recv(4096).decode("utf-8", "replace")
            if d: buf += d
            if marker in buf: return buf
        except socket.timeout: continue
    return buf

def send(line): s.sendall((line + "\n").encode())
def cap(cmd, timeout=30):
    send(f"printf '{S}\\n'; {cmd}; printf '{E}\\n'")
    out = read_until(E, timeout)
    return out.split(S,1)[1].rsplit(E,1)[0].strip() if S in out and E in out else out.strip()

read_until("Please press Enter", timeout=120)
send(""); time.sleep(1); send(""); time.sleep(1)
send("stty -echo 2>/dev/null; PS1="); time.sleep(1)

print("\n== /proc/cmdline =="); print(cap("cat /proc/cmdline"))
print("\n== dmesg virtio/drm/blob/virgl =="); print(cap("dmesg 2>/dev/null | grep -iE 'virtio_gpu|\\[drm\\]|resource_blob|virgl|host_visible|capset' | head -30"))
print("\n== fw_cfg sysfs =="); print(cap("ls /sys/firmware/qemu_fw_cfg/by_name 2>&1 | head; ls -d /sys/firmware/qemu_fw_cfg 2>&1"))
print("\n== drm sysfs features =="); print(cap("for f in /sys/class/drm/card*/device/features /sys/kernel/debug/dri/*/virtio*; do echo \"$f:\"; cat \"$f\" 2>&1; done | head -20"))
print("\n== virtio_gpu modinfo/blob param =="); print(cap("ls /sys/module/virtio_gpu/ 2>&1; cat /sys/class/drm/card1/device/uevent 2>&1 | head"))
print("\n== blob test via debugfs =="); print(cap("find /sys -iname '*blob*' 2>/dev/null | head; find /sys -iname '*host_visible*' 2>/dev/null | head"))

sh("docker", "rm", "-f", CNAME)
print("\n[*] done")

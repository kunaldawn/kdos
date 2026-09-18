#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Boot the ISO the way a legacy machine does: no UEFI firmware at all, the
# image attached as a USB stick, and nothing else to boot from.
#
# vnc-shot.py CANNOT ANSWER THIS QUESTION. It always passes `-bios OVMF.fd`, so
# every run it makes is a UEFI run — a BIOS regression would leave every rig
# test green. That is the whole reason this file exists beside it.
#
# THE CONTROLLER MUST BE xHCI. `-usb` on this machine type gives an emulated
# UHCI controller, which is USB 1.1: SeaBIOS will not boot the image from it and
# the run dies on its timeout with an EMPTY serial log, which reads exactly like
# an image with no boot code in its MBR. Measured: the same image that produced
# nothing over `-usb` reached a root shell over `qemu-xhci`.
#
# PROOF IS THE SERIAL LOG AND NOT THE EXIT STATUS. qemu is killed rather than
# shut down, so it always exits non-zero; what says the boot worked is the
# initramfs finding the medium and reaching switch_root.

set -e
cd "$(dirname "$0")/.."

ISO=build/iso-build/kdos.iso
OUT=build/shots
SERIAL=$OUT/bios-serial.log
SECS=${KDOS_BIOS_SECS:-90}

[ -f "$ISO" ] || { echo "no ISO at $ISO — run 'make build' first" >&2; exit 1; }
mkdir -p "$OUT"
rm -f "$SERIAL" "$OUT"/bios-screen.ppm

docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos kdos-qemu-py:latest \
    python3 - "$SECS" <<'PY'
import socket, subprocess, sys, time

secs = int(sys.argv[1])
q = subprocess.Popen([
    "qemu-system-x86_64", "-enable-kvm", "-cpu", "host", "-smp", "4", "-m", "2G",
    "-display", "none", "-vga", "std",
    "-device", "qemu-xhci,id=xhci",
    "-drive", "if=none,id=stick,format=raw,file=/kdos/build/iso-build/kdos.iso",
    "-device", "usb-storage,bus=xhci.0,drive=stick,removable=on",
    "-monitor", "unix:/tmp/mon.sock,server,nowait",
    "-serial", "file:/kdos/build/shots/bios-serial.log",
])
time.sleep(4)
s = socket.socket(socket.AF_UNIX)
s.connect("/tmp/mon.sock")
time.sleep(secs)
s.sendall(b"screendump /kdos/build/shots/bios-screen.ppm\n")
time.sleep(3)
q.terminate()
q.wait(timeout=10)
PY

fail=0
for want in "Found KDOS media" "Switching to new root"; do
    if ! grep -qa "$want" "$SERIAL" 2>/dev/null; then
        echo "FAIL: the serial log never said '$want'" >&2
        fail=1
    fi
done
if [ "$fail" = 0 ]; then
    echo "bios-boot: ok — booted from a USB stick with no UEFI firmware"
    echo "  serial  $SERIAL"
    echo "  screen  $OUT/bios-screen.ppm  (raw PPM despite any extension)"
else
    echo "bios-boot: FAILED — $(wc -c < "$SERIAL" 2>/dev/null || echo 0) bytes of serial" >&2
    exit 1
fi

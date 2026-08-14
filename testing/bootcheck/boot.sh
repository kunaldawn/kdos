#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/bootcheck/boot.sh — start the ISO headless, with a way in
#
# Boots build/iso-build/kdos.iso with the serial console and the qemu monitor on
# unix sockets and no display at all, so a session can be driven and looked at
# from a script. See README.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO=${KDOS_REPO:-$(cd "$HERE/../.." && pwd)}

# The sockets do NOT live beside this script. A unix socket path is capped at
# 108 bytes and qemu refuses a longer one outright ("UNIX socket path is too
# long"), which a deep checkout or a scratch directory reaches easily. Short
# path by default, overridable.
RUN=${KDOS_BOOTCHECK_RUN:-/tmp/kdos-bootcheck}
mkdir -p "$RUN"

ISO=${KDOS_ISO:-$REPO/build/iso-build/kdos.iso}
DISK=${KDOS_DISK:-$REPO/build/kdos.qcow2}
MODE=${1:-soft}			# soft = pixman (what `make run` gives) | gl = virgl
shift || true

[ -f "$ISO" ] || { echo "no ISO at $ISO — run 'make build'"; exit 1; }
[ -f "$DISK" ] || qemu-img create -f qcow2 "$DISK" 20G >/dev/null

rm -f "$RUN"/serial.log "$RUN"/mon.log "$RUN"/serial.sock "$RUN"/mon.sock

case "$MODE" in
  gl)   DISPLAY_ARGS="-display egl-headless -vnc ${KDOS_VNC:-:29} -device virtio-vga-gl" ;;
  soft) DISPLAY_ARGS="-display none -vnc ${KDOS_VNC:-:29} -device virtio-vga" ;;
  *)    echo "usage: boot.sh [soft|gl]"; exit 1 ;;
esac

# -boot d so the installed disk, once there is one, does not win over the ISO.
qemu-system-x86_64 \
  -enable-kvm -cpu host -smp 8 -m 8G \
  -bios /usr/share/ovmf/OVMF.fd \
  -cdrom "$ISO" -boot d \
  -drive file="$DISK",format=qcow2,if=virtio \
  -vga none $DISPLAY_ARGS \
  -usb -device usb-tablet \
  -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
  -serial unix:"$RUN"/serial.sock,server,nowait \
  -monitor unix:"$RUN"/mon.sock,server,nowait \
  -pidfile "$RUN"/qemu.pid \
  "$@" >"$RUN"/qemu.out 2>&1 &

sleep 2
# One owner per socket: a background `tail`-style reader competing with a
# foreground writer on the same socket desynchronises the console — the reason
# this is a daemon and not two shell redirections.
python3 "$HERE"/sock.py "$RUN"/serial.sock "$RUN"/serial >/dev/null 2>&1 &
python3 "$HERE"/sock.py "$RUN"/mon.sock "$RUN"/mon >/dev/null 2>&1 &
sleep 1
echo "booted mode=$MODE run=$RUN pid=$(cat "$RUN"/qemu.pid 2>/dev/null)"

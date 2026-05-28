#!/bin/bash
# Run KDOS under a containerized QEMU 10 (virgl+blob) rendering on the host
# NVIDIA GPU, with the GTK window on the host's XWayland display.
#
# The host's packaged QEMU is 8.2.2 (no blob+virgl, no venus); this sidesteps it
# entirely. `make run` / `make rundisk` still use the host QEMU + software-GL shell.
#
# Usage: testing/qemu-hw/run.sh [iso|disk]   (default: iso)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE="${1:-iso}"
IMAGE="kdos-qemu-hw"
ISO="$REPO_ROOT/build/iso-build/kdos.iso"
QCOW="$REPO_ROOT/build/kdos.qcow2"
MEM="4G"
DISPLAY_NUM="${DISPLAY:-:0}"

[ -f "$ISO" ] || { echo "ISO not found: $ISO (run 'make build' first)"; exit 1; }

# Build the QEMU-10 container image on first use.
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "[*] building $IMAGE image (first run)..."
    docker build -t "$IMAGE" "$(dirname "${BASH_SOURCE[0]}")"
fi
[ -f "$QCOW" ] || qemu-img create -f qcow2 "$QCOW" 20G 2>/dev/null || \
    docker run --rm -v "$REPO_ROOT/build:/build" "$IMAGE" -c "qemu-img create -f qcow2 /build/kdos.qcow2 20G"

# Display backend. On a Wayland host, QEMU's gtk,gl=on through XWayland from a
# container renders a BLACK window on NVIDIA — so present as a native Wayland
# client of the host compositor instead. Fall back to X11 elsewhere.
DISP_ARGS=()
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] && [ -n "${WAYLAND_DISPLAY:-}" ] \
   && [ -S "${XDG_RUNTIME_DIR:-}/$WAYLAND_DISPLAY" ]; then
    echo "[*] display: native Wayland ($WAYLAND_DISPLAY)"
    DISP_ARGS=(
        -e XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
        -e WAYLAND_DISPLAY="$WAYLAND_DISPLAY"
        -e GDK_BACKEND=wayland
        -v "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY":"$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    )
else
    echo "[*] display: X11 ($DISPLAY_NUM)"
    xhost +local:root >/dev/null 2>&1 || true
    trap 'xhost -local:root >/dev/null 2>&1 || true' EXIT
    DISP_ARGS=(
        -e DISPLAY="$DISPLAY_NUM"
        -e GDK_BACKEND=x11
        -e QT_X11_NO_MITSHM=1
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw
    )
fi

# GPU device: virtio-vga-gl with blob (HW dmabuf sharing — what makes the Qt
# shell render instead of blanking). RAM via memfd (share=on) so blob resources
# aren't capped by the host's udmabuf size_limit_mb. venus= is intentionally NOT
# set: it SIGABRTs the host EGL path and isn't needed — blob alone fixes Qt.
GPU_ARGS=(
    -object memory-backend-memfd,id=mem,size="$MEM",share=on
    -machine pc,memory-backend=mem,accel=kvm
    -m "$MEM"
    -cpu host
    -vga none
    -device virtio-vga-gl,blob=true,hostmem=4G,xres=1920,yres=1080
    -display gtk,gl=on
)

case "$MODE" in
    iso)  SRC_ARGS=(-cdrom /work/build/iso-build/kdos.iso -drive file=/work/build/kdos.qcow2,format=qcow2 -usb -device usb-tablet) ;;
    disk) SRC_ARGS=(-drive file=/work/build/kdos.qcow2,format=qcow2 -usb -device usb-tablet) ;;
    *)    echo "unknown mode: $MODE (use iso|disk)"; exit 1 ;;
esac

exec docker run --rm -it \
    --gpus all \
    --device /dev/kvm \
    --device /dev/dri \
    --device /dev/udmabuf \
    "${DISP_ARGS[@]}" \
    -v /usr/share/ovmf:/usr/share/ovmf:ro \
    -v "$REPO_ROOT/build:/work/build" \
    "$IMAGE" -c "exec qemu-system-x86_64 -enable-kvm \
        -bios /usr/share/ovmf/OVMF.fd \
        ${GPU_ARGS[*]} \
        ${SRC_ARGS[*]} \
        -serial stdio \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0"

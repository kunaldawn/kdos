#!/bin/bash
# Run KDOS under a containerized QEMU 10 (virgl+blob) rendering on the host
# NVIDIA GPU, presenting as a native Wayland window (or X11 on an X11 host).
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
#
# gl=es, NOT gl=on: gl=on makes QEMU's gtk-egl path ask for a desktop-GL (core)
# context, which presents a BLACK window on this host (NVIDIA + Xorg) — the guest
# is fine, only the host blit is lost. gl=es asks for a GLES context and presents
# correctly, with virgl+blob still HW-accelerated.
#
# A compositor CAN wedge the whole guest (soft-lockup storm, vCPUs spinning on
# the virtio-gpu queue) through the gtk,gl=es path on this host — measured with
# cosmic-comp, and the storm is in the host/guest virtio-gpu path rather than in
# any one compositor, so assume it still applies. `egl-headless` is rock solid.
# If the desktop freezes the VM, run headless:
#   KDOS_QEMU_DISPLAY=egl-headless testing/qemu-hw/run.sh iso   (view via -vnc)
GPU_ARGS=(
    -object memory-backend-memfd,id=mem,size="$MEM",share=on
    -machine pc,memory-backend=mem,accel=kvm
    -m "$MEM"
    -cpu host
    -smp "$(nproc)"
    -vga none
    -device virtio-vga-gl,blob=true,hostmem=4G,xres=1920,yres=1080
    -display "${KDOS_QEMU_DISPLAY:-gtk,gl=es}"
)

# Audio. The container's QEMU is not the host's, so the backend is probed
# INSIDE it by the same testing/qemu-audio.sh every `make run*` uses — the host
# probe would be answering for the wrong binary. Only the socket has to come
# from out here.
AUDIO_ARGS=()
if [ -S "${XDG_RUNTIME_DIR:-}/pipewire-0" ]; then
    AUDIO_ARGS=(-e XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
                -v "$XDG_RUNTIME_DIR/pipewire-0":"$XDG_RUNTIME_DIR/pipewire-0")
elif [ -S "${XDG_RUNTIME_DIR:-}/pulse/native" ]; then
    AUDIO_ARGS=(-e XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
                -v "$XDG_RUNTIME_DIR/pulse/native":"$XDG_RUNTIME_DIR/pulse/native")
else
    echo "[*] audio: no host socket — the guest gets no sound card"
fi

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
    "${AUDIO_ARGS[@]}" \
    -v /usr/share/ovmf:/usr/share/ovmf:ro \
    -v "$REPO_ROOT/build:/work/build" \
    -v "$REPO_ROOT/testing:/work/testing:ro" \
    "$IMAGE" -c "exec qemu-system-x86_64 -enable-kvm \
        -bios /usr/share/ovmf/OVMF.fd \
        ${GPU_ARGS[*]} \
        ${SRC_ARGS[*]} \
        -serial stdio \
        \$(/work/testing/qemu-audio.sh) \
        -netdev user,id=net0 -device virtio-net-pci,netdev=net0"

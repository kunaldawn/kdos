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
# Print the QEMU arguments that give the guest a sound card, or nothing at all
# if the host has no way to play it. Every `make run*` target and the
# containerized run.sh share this, so the guest's audio hardware is the same
# everywhere: an Intel HDA controller, which the kernel's SND_HDA_INTEL module
# claims by itself.
#
# The backend is PROBED rather than hardcoded. QEMU aborts at startup on an
# -audiodev its build does not carry, so a wrong guess here does not degrade
# to silence — it stops the VM from booting at all.
#
# "Carries the driver" is necessary but NOT sufficient, which cost a debug
# cycle: the qemu-hw container installs qemu-system-x86, which pulls in
# libpipewire, so `-audiodev help` lists pipewire — but libpipewire cannot
# build a context without /usr/share/pipewire/client.conf, packaged separately
# in libpipewire-0.3-common. QEMU died with "Could not create PipeWire context"
# and took `make run-hw` with it. Each backend is therefore checked for the
# runtime files it actually needs, not just for its presence in the build.

set -u

QEMU="${QEMU_BIN:-qemu-system-x86_64}"
RUNTIME="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

command -v "$QEMU" >/dev/null 2>&1 || exit 0

have_driver() {
    "$QEMU" -audiodev help 2>/dev/null | grep -qx "$1"
}

# libpipewire reads client.conf through its own config path before it will
# create a context. PIPEWIRE_CONFIG_DIR overrides the compiled-in location, so
# honour it rather than assuming /usr/share.
have_pipewire_conf() {
    for d in "${PIPEWIRE_CONFIG_DIR:-}" /etc/pipewire /usr/share/pipewire; do
        [ -n "$d" ] && [ -f "$d/client.conf" ] && return 0
    done
    return 1
}

backend=""
if [ -S "$RUNTIME/pipewire-0" ] && have_driver pipewire && have_pipewire_conf; then
    backend=pipewire
elif [ -S "$RUNTIME/pulse/native" ] && have_driver pa; then
    backend=pa
elif [ -e /dev/snd/controlC0 ] && have_driver alsa; then
    backend=alsa
fi

[ -n "$backend" ] || exit 0

echo "-audiodev $backend,id=snd0 -device intel-hda -device hda-output,audiodev=snd0"

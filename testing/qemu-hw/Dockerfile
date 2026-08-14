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
# QEMU 10 + virglrenderer host harness for HW-accelerated KDOS testing.
# Ubuntu 25.10 ships QEMU 10.x (virgl+blob and venus), unlike the host's 8.2.2.
# Renders virgl on the real GPU via the NVIDIA Container Toolkit (run with
# --gpus all + NVIDIA_DRIVER_CAPABILITIES=all). Display goes to the host's
# XWayland over the mounted X11 socket. See run.sh.

FROM ubuntu:25.10

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        qemu-system-x86 \
        qemu-system-gui \
        qemu-system-modules-opengl \
        qemu-utils \
        ovmf \
        seabios \
        libvirglrenderer1 \
        libgl1 \
        libegl1 \
        libgles2 \
        libglx0 \
        libglvnd0 \
        libgbm1 \
        libgtk-3-0t64 \
        libepoxy0 \
        libvulkan1 \
        mesa-vulkan-drivers \
        vulkan-tools \
        mesa-utils \
        libnvidia-egl-wayland1 \
        libwayland-egl1 \
        libwayland-client0 \
        pciutils \
        # qemu-system-x86 pulls in libpipewire, so `-audiodev help` lists
        # pipewire and the probe picks it — but libpipewire will not build a
        # context without /usr/share/pipewire/client.conf, and that file ships
        # in the `pipewire` package, NOT in libpipewire-0.3-0t64 or the
        # -common package (which on 25.10 carries only documentation). Without
        # it QEMU dies at startup with "Could not create PipeWire context" and
        # `make run-hw` never reaches the guest. The daemon it also installs is
        # never started here; only its config is wanted.
        pipewire \
    && rm -rf /var/lib/apt/lists/*

# glvnd dispatch finds the NVIDIA libs the container toolkit injects at runtime.
ENV NVIDIA_DRIVER_CAPABILITIES=all
ENV NVIDIA_VISIBLE_DEVICES=all

ENTRYPOINT ["/bin/bash"]

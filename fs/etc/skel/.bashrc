# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ~/.bashrc — read by every interactive non-login bash (foot, distrobox enter).
#
# $HOME is bind-mounted into every distrobox, so this one file configures both
# sides. On the host it sources /etc/bash.bashrc; inside a box that path is the
# container's own file, so the host's is read through /run/host instead — the
# live file, not a copy that can go stale.
if [ -n "${CONTAINER_ID:-}" ] && [ -r /run/host/etc/bash.bashrc ]; then
    . /run/host/etc/bash.bashrc
elif [ -r /etc/bash.bashrc ]; then
    . /etc/bash.bashrc
fi

# Your own aliases and prompt overrides go below.

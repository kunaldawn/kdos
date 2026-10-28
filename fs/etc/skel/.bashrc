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

# ls/grep colours, generated per accent by `kdos theme` (it exports
# LS_COLORS). Guarded: a home the generator has not reached yet keeps the
# tools' defaults rather than printing an error per shell. Under
# $XDG_CONFIG_HOME, because that is where `kdos theme` writes it.
_kdos_lsc="${XDG_CONFIG_HOME:-$HOME/.config}/kdos/ls-colors"
[ -r "$_kdos_lsc" ] && . "$_kdos_lsc"
unset _kdos_lsc

# Your own aliases and prompt overrides go below.

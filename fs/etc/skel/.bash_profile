# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ~/.bash_profile — read by login bash (tty, serial, ssh) after /etc/profile.
# Pull in the interactive config so a login shell behaves like a terminal one.
[ -r "$HOME/.bashrc" ] && . "$HOME/.bashrc"

# THE CONSOLE DESKTOP IS THE DEFAULT SESSION, on tty1 and nowhere else. tty2 is
# the recovery console and must stay a shell; a serial line and an ssh session
# have no console to take.
#
# NOT exec'd. A session that fails to come up leaves this shell to fall through
# to a prompt, which is the difference between a machine you can fix and one
# that shows a message and takes the tty with it.
if [ -z "$KDOS_CON" ] && [ -z "$WAYLAND_DISPLAY" ] && \
   [ "$(tty 2>/dev/null)" = /dev/tty1 ] && \
   command -v kdos-con-start >/dev/null 2>&1; then
	kdos-con-start
fi

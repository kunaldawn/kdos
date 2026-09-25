# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ~/.zprofile — read by login zsh after /etc/zsh/zprofile, which runs
# /etc/profile. The same session start as ~/.bash_profile, so an account whose
# login shell is zsh still gets the desktop.
#
# THE DESKTOP IS THE DEFAULT SESSION, on tty1 and nowhere else. tty2 is the
# recovery console and must stay a shell; a serial line and an ssh session have
# no screen to take.
#
# NOT exec'd. A session that fails to come up leaves this shell to fall through
# to a prompt, which is the difference between a machine you can fix and one
# that shows a message and takes the tty with it.
if [ -z "$WAYLAND_DISPLAY" ] && \
   [ "$(tty 2>/dev/null)" = /dev/tty1 ] && \
   command -v kdos-desktop >/dev/null 2>&1; then
	kdos-desktop
fi

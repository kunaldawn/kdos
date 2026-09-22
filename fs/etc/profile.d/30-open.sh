# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ONE ROAD TO A LINK. $BROWSER is what a great many terminal programs consult
# before anything else, and `xdg-open` on this image is `kdos-appbox open` — so
# pointing the variable at it makes the variable and the mimeapps table the
# same answer instead of two that drift.
#
# NOT OVER A VALUE SOMEBODY SET. A person who exports their own $BROWSER has
# said what they want and this must not argue.
#
# THIS FILE IS READ BY A LOGIN SHELL ONLY, which is the only way into a session
# here: /etc/inittab hands tty1 to agetty and the shell's profile starts the
# desktop, so anything the desktop needs defaulted can be defaulted here.
export BROWSER="${BROWSER:-xdg-open}"

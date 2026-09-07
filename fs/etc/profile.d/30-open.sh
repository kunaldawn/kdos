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
# THIS FILE IS READ BY A LOGIN SHELL ONLY. The console session on the `greet =
# yes` path is exec'd from a program that clears the environment first, so
# kdos-con-start defaults the same variable for that path; neither place
# overrides the other, because both only fill a gap.
export BROWSER="${BROWSER:-xdg-open}"

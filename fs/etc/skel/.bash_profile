# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# ~/.bash_profile — read by every login bash after /etc/profile: a console, the
# serial line, ssh, and the login shell kdos-session-wrapper starts Plasma
# from. Pull in the interactive config so a login shell behaves like a
# terminal one.
#
# WHAT IS ADDED HERE RUNS BEFORE THE DESKTOP. The desktop's login reads this
# file before Plasma starts, so a command here that waits holds the session
# back behind the login screen, and one that prints writes into the session's
# log rather than to anyone.
[ -r "$HOME/.bashrc" ] && . "$HOME/.bashrc"

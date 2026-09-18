# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE ACCENT, FOR THE PROGRAMS THAT TAKE IT IN THE ENVIRONMENT.
#
# fzf has no configuration file and no include: its colours are command-line
# flags, and the only thing it reads without being told is $FZF_DEFAULT_OPTS.
# So `kdos theme` writes the flags and this puts them where fzf will see them.
#
# APPENDED, NEVER ASSIGNED. A person may have their own $FZF_DEFAULT_OPTS —
# a layout, a keybinding, --height — and a profile script that overwrote it
# would take those away to deliver a colour.
if [ -r "${XDG_CONFIG_HOME:-$HOME/.config}/kdos/fzf-colors" ]; then
	. "${XDG_CONFIG_HOME:-$HOME/.config}/kdos/fzf-colors"
	FZF_DEFAULT_OPTS="${FZF_DEFAULT_OPTS:+$FZF_DEFAULT_OPTS }$KDOS_FZF_COLORS"
	export FZF_DEFAULT_OPTS
	unset KDOS_FZF_COLORS
fi

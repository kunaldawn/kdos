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

_mk=(CFLAGS="$CFLAGS"
	prefix=/usr
	gitexecdir=/usr/lib/git-core
	perllibdir="$(/usr/bin/perl -MConfig -wle 'print $Config{installvendorlib}')"
	NO_REGEX=NeedsStartEnd
	NO_TCLTK=Yes
	NO_RUST=Yes
	USE_LIBPCRE2=Yes
	DESTDIR=$PKG)
make "${_mk[@]}" install install-man
make -C contrib/subtree "${_mk[@]}" install install-man

# No git-completion.zsh: installed as site-functions/_git it comes first in
# fpath and shadows zsh's own, fuller _git with a wrapper around the bash one.
install -Dm644 contrib/completion/git-completion.bash "$PKG/usr/share/bash-completion/completions/git"
install -Dm644 contrib/completion/git-prompt.sh -t "$PKG/usr/share/git"

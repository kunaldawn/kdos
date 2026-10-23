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

_vendor=$(/usr/bin/perl -MConfig -wle 'print $Config{installvendorlib}')
_mk=(CFLAGS="$CFLAGS"
	LDFLAGS="$LDFLAGS"
	prefix=/usr
	gitexecdir=/usr/lib/git-core
	perllibdir="$_vendor"
	PYTHON_PATH=/usr/bin/python3
	NO_REGEX=NeedsStartEnd
	NO_TCLTK=Yes
	NO_RUST=Yes
	USE_LIBPCRE2=Yes
	DESTDIR=$PKG)
make "${_mk[@]}" install install-man
make -C contrib/subtree "${_mk[@]}" install install-man

rm -r "$PKG$_vendor/Git/SVN" "$PKG$_vendor/Git/SVN.pm"
rm "$PKG"/usr/lib/git-core/git-{svn,cvsserver} "$PKG/usr/bin/git-cvsserver" \
	"$PKG"/usr/share/man/man1/git-{svn,cvsserver}.1

install -Dm644 contrib/completion/git-completion.bash "$PKG/usr/share/bash-completion/completions/git"
install -Dm644 contrib/completion/git-prompt.sh -t "$PKG/usr/share/git"

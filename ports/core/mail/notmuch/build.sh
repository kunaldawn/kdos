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

_sphinx=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' /usr/lib/python3-sphinx)
PYTHONPATH=$_sphinx ./configure --prefix=/usr --libdir=/usr/lib \
	--with-docs \
	--with-api-docs \
	--with-bash-completion \
	--with-zsh-completion \
	--with-retry-lock \
	--without-emacs \
	--without-desktop \
	--without-ruby \
	--without-rpath
make
make DESTDIR=$PKG install
install -m755 notmuch-git "$PKG/usr/bin/notmuch-git"
ln -s notmuch-git "$PKG/usr/bin/nmbug"
(
	cd bindings/python-cffi
	pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root="$PKG" --prefix=/usr .
)
test -n "$(find "$PKG"/usr/lib/python3*/site-packages/notmuch2 -maxdepth 1 -name '_capi*.so')"
compgen -G "$PKG/usr/share/man/man1/notmuch.1*" > /dev/null

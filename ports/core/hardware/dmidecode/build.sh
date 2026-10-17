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

make prefix=/usr
make prefix=/usr compdir=/usr/share/bash-completion/completions \
	DESTDIR=$PKG install

for f in completion/*.bash; do
	p=${f##*/}; p=${p%.bash}
	if [ -e "$PKG/usr/sbin/$p" ]; then
		install -Dm644 "$f" "$PKG/usr/share/bash-completion/completions/$p"
	fi
done

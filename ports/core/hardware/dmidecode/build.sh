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

# kdos-resctl parses the SMBIOS table itself for the two fields the monitor
# shows, deliberately, because a setuid helper must not exec anything. This is
# the other half: the WHOLE table, for a person trying to find out what memory
# a machine takes with no model number on the case and no web to look it up in.
make prefix=/usr
make prefix=/usr compdir=/usr/share/bash-completion/completions \
	DESTDIR=$PKG install

# install-completion copies only when the BUILD host already has the
# completion directory, so the result would hang on whether bash-completion
# happened to be installed first. Every program built gets its completion.
for f in completion/*.bash; do
	p=${f##*/}; p=${p%.bash}
	if [ -e "$PKG/usr/sbin/$p" ]; then
		install -Dm644 "$f" "$PKG/usr/share/bash-completion/completions/$p"
	fi
done

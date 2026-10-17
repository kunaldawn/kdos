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

GIT_DIR="$SRC/.no-git" gem build asciidoctor.gemspec

_gemdir=$(ruby -e 'print Gem.default_dir')
gem install \
	--local \
	--ignore-dependencies \
	--no-user-install \
	--no-document \
	--install-dir "$PKG$_gemdir" \
	--bindir "$PKG/usr/bin" \
	"$name-$version.gem"

rm -rf "$PKG$_gemdir/cache"

install -Dm644 man/asciidoctor.1 -t "$PKG/usr/share/man/man1"

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

# THE GEM IS BUILT FROM THIS TREE AND INSTALLED FROM THAT FILE, with nothing
# resolved: --local keeps rubygems off the network and asciidoctor has no
# runtime dependencies to ignore. Installing it as a gem rather than copying
# lib/ is what puts it in `gem list` and gives it the stub rubygems writes.
#
# The gemspec lists its files with `git ls-files` and falls back to the tree
# only when that prints nothing. A GIT_DIR that does not exist makes it print
# nothing wherever the build directory sits, including inside a checkout.
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

# The cached .gem is a second copy of what is unpacked beside it.
rm -rf "$PKG$_gemdir/cache"

install -Dm644 man/asciidoctor.1 -t "$PKG/usr/share/man/man1"

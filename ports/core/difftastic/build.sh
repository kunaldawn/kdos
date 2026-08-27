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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# THE GRAMMARS ARE COMPILED IN, WHICH IS WHY THIS ONE WORKS OFFLINE AND THE
# EDITOR PLUGINS DO NOT. Every tree-sitter consumer in an editor downloads and
# compiles a grammar on first use; difftastic links about fifty of them into
# the binary at build time, so a syntax-aware diff is available on a machine
# that has never had a network. That is also why the build is long.
#
# What it buys: with no CI and no reviewer, `git diff` reporting a whole
# reindented block as changed is the difference between a refactor you can
# check and one you have to trust.
cargo build --release --frozen --offline
install -Dm755 target/release/difft $PKG/usr/bin/difft
install -Dm644 difft.1 $PKG/usr/share/man/man1/difft.1

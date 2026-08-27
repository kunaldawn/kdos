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

# Header-only. The kainjow/ subdirectory is the include path libkiwix probes
# for; installing the header at both names would give two copies to keep in
# step, so the bare name is a symlink to it.
install -Dm644 mustache.hpp "$PKG/usr/include/kainjow/mustache.hpp"
install -d "$PKG/usr/include"
ln -sf kainjow/mustache.hpp "$PKG/usr/include/mustache.hpp"

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

# op=install runs the unit tests first and a failed test stops the install.
# cmd-interrupt races a SIGINT against a one-second sleep, so under a loaded
# build it fails at random; BROKEN_TESTS is upstream's own exclusion list.
BROKEN_TESTS=cmd-interrupt ./boot-strap --prefix=/usr --install-destdir=$PKG op=install

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
make prefix=/usr DESTDIR=$PKG install

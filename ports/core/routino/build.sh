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

# Routino has no configure: the Makefile reads a config file, and every path is
# a make variable. The router is what runs on a booted machine; planetsplitter
# is what turns an .osm extract into the database it reads, and both ship
# because an extract with no preprocessing step is a file nothing can use.
make prefix=/usr
make prefix=/usr DESTDIR=$PKG install

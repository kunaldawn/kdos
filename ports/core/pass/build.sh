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

# 900 LINES OF bash AND FROZEN SINCE 2021, WHICH IS THE ARGUMENT FOR IT. A
# password manager is the one program on a machine you cannot afford to have a
# surprise in, and this one is a file per entry in a git repository with gpg
# over each — no database, no format to migrate, and readable with `gpg -d` if
# every line of pass itself disappeared. That last property is what makes it
# the right store on a distro whose whole premise is that the machine keeps
# working when nothing else is reachable.
#
# `tree` IS A HARD DEPENDENCY, not a nicety: `pass` with no argument lists the
# store by running it, and without it the command that shows you what you have
# prints nothing.
make PREFIX=/usr WITH_ALLCOMP=yes DESTDIR=$PKG install

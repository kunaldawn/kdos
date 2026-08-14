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

# The package ships `packages/freedesktop.org.xml` and NOTHING ELSE: the build
# passes --disable-update-mimedb, so the compiled database — `globs`, `types`,
# `aliases`, `subclasses`, `mime.cache` — is never generated. Measured on a
# booted ISO: /usr/share/mime contained one directory, and every consumer that
# asks "what type is this file" got no answer at all.
#
# That is what this hook is for. It runs on the TARGET, where the binary the
# package just installed exists, which is exactly the reason the build cannot
# do it: a cross build has no target update-mime-database to run.
update-mime-database /usr/share/mime

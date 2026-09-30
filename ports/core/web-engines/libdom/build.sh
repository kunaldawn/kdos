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

# The HTML binding parses through libhubbub and the XML binding through expat;
# libxml2's binding is the alternative to expat and only one is built.

# Every NetSurf library's makefile adds -Werror, and the environment's CFLAGS
# come after it, so -Wno-error keeps a warning a newer compiler adds from
# failing the build.
export CFLAGS="$CFLAGS -Wno-error"
export NSSHARED=/usr/share/netsurf-buildsystem
make COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib WITH_HUBBUB_BINDING=yes WITH_EXPAT_BINDING=yes WITH_LIBXML_BINDING=no
# The build system's INSTALL is `install -C`, which toybox's install rejects;
# -C only skips identical files, and $PKG starts empty.
make install INSTALL=install COMPONENT_TYPE=lib-shared PREFIX=/usr LIBDIR=lib WITH_HUBBUB_BINDING=yes WITH_EXPAT_BINDING=yes WITH_LIBXML_BINDING=no DESTDIR=$PKG

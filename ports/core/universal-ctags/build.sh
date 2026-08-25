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

./autogen.sh

# The four optional libraries are what the parsers need and each is answered by
# silently dropping support rather than by failing: without jansson there is no
# JSON output for an editor to read, without libxml2 no XML/XSLT/Maven parsing,
# without pcre2 no multiline regex parsers, and without yaml no Ansible or
# OpenAPI. A ctags that claims 140 languages and parses 130 is the failure.
./configure \
	--prefix=/usr \
	--enable-json \
	--enable-xml \
	--enable-yaml \
	--enable-pcre2
make
make DESTDIR=$PKG install

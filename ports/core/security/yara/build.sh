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

./bootstrap.sh

# THE POINT IS THAT A RULE IS TEXT. An antivirus signature blob says a file is
# bad and gives you nothing to check; a YARA rule is a few lines naming the
# strings and the conditions, so somebody with no network can read WHY a file
# matched and decide whether they agree. That is the whole reason this is here
# rather than a scanner.
#
# IT SHIPS INERT. There are no rules in this package and none are shipped: a
# rule set is somebody's judgement about what is suspicious and it decays from
# the day it is written, so pointing this at a curated set is a decision for
# whoever runs it. `yara` with no rules matches nothing and says so.
#
# THE `magic` MODULE IS FORCED ON: it links file(1)'s libmagic, and without it
# a rule using `magic.type` fails at load time rather than at build time, which
# is the wrong end to find out. --enable-magic makes a missing libmagic stop
# configure. cuckoo stays off: it reads Cuckoo sandbox reports, which this
# machine has no source for. dotnet parses the scanned file itself and needs
# no library, so it stays on.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--enable-macho \
	--enable-dex \
	--enable-magic \
	--disable-cuckoo \
	--with-crypto
make
make DESTDIR=$PKG install

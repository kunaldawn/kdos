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

# ONLY WHAT TOYBOX IMPLEMENTS TOO NARROWLY IS INSTALLED, and today that is one
# program. toybox is the userland by rule; this exists for the same reason
# `sed`, `gawk` and `findutils` do — an upstream build system reaches for a GNU
# extension, toybox answers with nothing rather than an error, and the failure
# lands somewhere else entirely.
#
# `expr length STRING` is the case. POSIX does not define it and toybox does
# not implement it, so it yields the EMPTY STRING; brltty's configure then does
# `test "" -eq 2`, prints `integer expected`, falls through its driver lookup
# and reports `unknown speech driver: eSpeak-NG` — a message about a driver
# that is present, caused by a string function that is not.
#
# GROWING THIS IS ADDING A NAME TO THE LIST BELOW, not renaming the package.
# Installing all of coreutils would take ~100 paths off toybox, which is the
# opposite of what this distro is. These two are named because a toybox applet
# is genuinely missing the feature a port asks for, the same reason GNU sed,
# gawk and findutils sit over their applets: toybox `expr` has no `length`, and
# toybox `ln` has no `--relative`, which meson install scripts use to make a
# symlink inside DESTDIR that is still correct once the tree is moved.
INSTALL_PROGRAMS="expr ln"

# FORCE_UNSAFE_CONFIGURE because kpkg builds as root in a chroot. The check
# exists because one mknod probe would pass as root and fail for a normal
# user, baking in a wrong answer — which does not apply to `expr`, and the
# whole of what this port installs is `expr`.
export FORCE_UNSAFE_CONFIGURE=1

./configure --prefix=/usr --disable-nls --without-selinux
make

for prog in $INSTALL_PROGRAMS; do
	install -Dm755 "src/$prog" "$PKG/usr/bin/$prog"
done

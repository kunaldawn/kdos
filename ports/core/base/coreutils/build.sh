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

# ONLY WHAT TOYBOX IMPLEMENTS TOO NARROWLY IS INSTALLED, and that is three
# programs. toybox is the userland by rule; this exists for the same reason
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
# opposite of what this distro is. These three are named because a toybox applet
# is genuinely missing the feature a port asks for, the same reason GNU sed,
# gawk and findutils sit over their applets: toybox `expr` has no `length`;
# toybox `ln` has no `--relative`, which meson install scripts use to make a
# symlink inside DESTDIR that is still correct once the tree is moved; and
# toybox `printf` has no `%q`, which KOReader's build runs as an external
# command (`exec printf`) to quote every logged command line, and stops on.
INSTALL_PROGRAMS="expr ln printf"

# FORCE_UNSAFE_CONFIGURE because kpkg builds as root in a chroot. The check
# exists because one mknod probe would pass as root and fail for a normal
# user, baking in a wrong answer — which applies to none of the programs this
# port installs.
export FORCE_UNSAFE_CONFIGURE=1

# `expr` does its big-integer arithmetic through libgmp when configure finds it
# and through gnulib's bundled mini-gmp when it does not. --with-libgmp makes a
# missing gmp stop configure, so the binary links the gmp `depends` names
# instead of changing with whatever the build root carries.
./configure --prefix=/usr --disable-nls --without-selinux --with-libgmp
make

for prog in $INSTALL_PROGRAMS; do
	install -Dm755 "src/$prog" "$PKG/usr/bin/$prog"
	install -Dm644 "man/$prog.1" -t "$PKG/usr/share/man/man1"
done

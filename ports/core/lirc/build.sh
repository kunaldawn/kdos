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

# Alpine's patches: the bundled lirc.org pages without their third-party
# advertising; sys/sysmacros.h for major() and minor(), which musl
# does not reach through sys/types.h; no integer initialiser for pthread_t,
# which GCC 14 and later reject; and the input_event time fields read through
# the kernel's y2038-safe names.
patch -p1 -i "$PORT_SRC/0001-lirc.org-Remove-non-free-advertising.patch"
patch -p1 -i "$PORT_SRC/0008-fix-event-time-on-32bit.patch"
patch -p1 -i "$PORT_SRC/0010-lirc-add-include-for-major.patch"
patch -p1 -i "$PORT_SRC/musl-gcc14.patch"

# devinput, uinput and the lock directory default to whatever the build host
# has under /dev and /var/lock, so all three are named. The socket directory
# and the lock directory are under /run, which /var/run and /var/lock only
# link to. --without-x drops irxevent and xmode2, which drive an X server. The
# HID, FTDI, ALSA and PortAudio receiver plugins are found by probing; each
# library they need is in depends so none of them drops out.
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --runstatedir=/run \
	--libdir=/usr/lib --disable-static \
	--enable-devinput \
	--enable-uinput \
	--with-lockdir=/run/lock \
	--with-systemdsystemunitdir=no \
	--without-x
make
make DESTDIR=$PKG install

# configure installs systemd units whatever the flag says when the build
# host's /proc/version names Ubuntu, and that file belongs to the host kernel
# the chroot runs on. No unit belongs on this system. The install also makes
# /var/run/lirc, a path through the /var/run link that a package cannot own;
# lircd does not create its socket directory, so whatever starts it makes
# /run/lirc first.
rm -rf "$PKG/lib/systemd" "$PKG/usr/lib/systemd" "$PKG/var/run"

# lirc-setup is a GTK 3 configuration wizard whose remote and driver lists
# come from lirc-remotes.sourceforge.net, so offline it has nothing to offer.
# The lircd.conf files it would fetch are placed by hand under
# /etc/lirc/lircd.conf.d instead.
rm -rf "$PKG/usr/bin/lirc-setup" "$PKG"/usr/lib/python3*/site-packages/lirc-setup \
	"$PKG/usr/share/man/man1/lirc-setup.1"

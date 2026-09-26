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

# THE TARBALL IS FLAT — no wrapping directory — and kpkg strips one component
# from a first source unconditionally, which on this archive removes every
# top-level file and promotes the subdirectories' contents into their place.
# `meson.build` is one of the files that disappears, so the build stops before
# it starts. Unpacking it here is what the rule asks for; testing/preflight.sh
# checks that every flat first source has this line.
tar xzf "$PORT_SRC/${name}-${version}.tar.gz"

# musl declares `struct statx` in <sys/stat.h> and the kernel headers declare
# it too, so io_uring_engine.h's <linux/stat.h> is a redefinition in every file
# that reaches it. liburing.h already brings in <sys/stat.h>, so the kernel
# header goes.
patch -p1 -i "$PORT_SRC/musl-statx.patch"

# The index is per user and named by LOCATE_PATH (/etc/profile.d/40-plocate.sh);
# there is no shared database for the compiled-in default to find, and plocate
# stops at the first database it cannot open.
patch -p1 -i "$PORT_SRC/locate-path-replaces-default.patch"

# liburing is `required: false` upstream and has no option, so the `depends`
# line is what turns the io_uring lookup path on; without liburing installed
# the build silently takes its own pread path instead.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	-Dinstall_systemd=false \
	-Dinstall_cron=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# THE SETGID BIT IS REMOVED, AND THE DECISION IS IN
# docs/kdos/03-architecture/security-model.md. Upstream installs the binary
# `rwxr-sr-x root:plocate` so one shared index in /var/lib can be read on an
# unprivileged user's behalf, with a per-path `access()` check standing between
# that user and every path on the machine. KDOS builds the index PER USER
# instead — it can only hold what that user could already list — so the group,
# the shared file and the check all have nothing left to do, and the bit would
# be an privilege this binary never uses.
#
# The `plocate` group does not exist in fs/etc/group either, so `install` has
# already fallen back to root:root; this is what makes that deliberate rather
# than incidental.
chmod 0755 "$PKG/usr/bin/plocate"

# `locate` is plocate. findutils builds without its own locate and updatedb,
# so this name and plocate's /usr/sbin/updatedb are the only ones on $PATH.
ln -s plocate "$PKG/usr/bin/locate"

# And the shared database directory goes with it: nothing writes /var/lib/plocate
# on this image, and an empty root-owned directory is an invitation to put a
# system-wide index in it.
rm -rf "$PKG/var/lib/plocate"
rmdir --ignore-fail-on-non-empty "$PKG/var/lib" 2>/dev/null || true

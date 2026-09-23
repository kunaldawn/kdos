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

meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D i18n=disabled \
	-D docs=disabled \
	-D audit=disabled \
	-D econf=disabled \
	-D logind=disabled \
	-D elogind=disabled \
	-D selinux=disabled \
	-D nis=disabled \
	-D pam_userdb=disabled \
	-D examples=false \
	-D xtests=false \
	-D pam_unix=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
# pam_namespace.service is installed whatever the options: systemdunitdir
# only moves it, and nothing here reads a unit.
rm -rf "$PKG/usr/lib/systemd"

# unix_chkpwd is how pam_unix reads a 0600 /etc/shadow on behalf of a caller
# that is not root. meson installs it 0755; left that way it cannot open the
# file and every check from a session-user consumer — wayvnc, a lock screen —
# is refused.
chmod -v 4755 $PKG/usr/sbin/unix_chkpwd

# The stacks every other service includes by name. A service file naming one
# that is absent falls through to `other` -> pam_deny, which reports itself as
# "PAM account management error" and never mentions the missing stack.
install -vdm755 $PKG/etc/pam.d
cat > $PKG/etc/pam.d/system-auth << "EOF"
# Begin /etc/pam.d/system-auth
account   required   pam_unix.so
auth      required   pam_unix.so  nullok
auth      optional   pam_permit.so
session   required   pam_limits.so
session   required   pam_unix.so
password  required   pam_unix.so  yescrypt shadow try_first_pass
# End /etc/pam.d/system-auth
EOF

cat > $PKG/etc/pam.d/system-account << "EOF"
# Begin /etc/pam.d/system-account
account   required   pam_unix.so
# End /etc/pam.d/system-account
EOF

cat > $PKG/etc/pam.d/system-session << "EOF"
# Begin /etc/pam.d/system-session
session   required   pam_limits.so
session   required   pam_unix.so
# End /etc/pam.d/system-session
EOF

cat > $PKG/etc/pam.d/system-password << "EOF"
# Begin /etc/pam.d/system-password
password  required   pam_unix.so  yescrypt shadow try_first_pass
# End /etc/pam.d/system-password
EOF

# The catch-all: a service with no file of its own denies, and says so in the
# log, rather than authenticating against nothing.
cat > $PKG/etc/pam.d/other << "EOF"
# Begin /etc/pam.d/other
auth        required        pam_warn.so
auth        required        pam_deny.so
account     required        pam_warn.so
account     required        pam_deny.so
password    required        pam_warn.so
password    required        pam_deny.so
session     required        pam_warn.so
session     required        pam_deny.so
# End /etc/pam.d/other
EOF

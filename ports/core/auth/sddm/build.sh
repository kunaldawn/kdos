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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/sddm-elogind-session-desktop.patch"
patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/sddm-switch-user-greeter.patch"

export CXXFLAGS="$CXXFLAGS -I/usr/include/utmps"

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_WITH_QT6=ON \
	-D NO_SYSTEMD=ON \
	-D USE_ELOGIND=ON \
	-D ENABLE_JOURNALD=OFF \
	-D INSTALL_PAM_CONFIGURATION=OFF \
	-D BUILD_MAN_PAGES=ON \
	-D UID_MIN=1000 \
	-D UID_MAX=60000 \
	-D RUNTIME_DIR=/run/sddm \
	-D DBUS_CONFIG_FILENAME=sddm_org.freedesktop.DisplayManager.conf \
	-D CMAKE_CXX_STANDARD_LIBRARIES=-lutmps \
	-Wno-dev
grep -qx 'ELOGIND_FOUND:INTERNAL=1' build/CMakeCache.txt
cmake --build build
DESTDIR=$PKG cmake --install build

install -vdm755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/sddm" << "EOF_PAM"
# Begin /etc/pam.d/sddm
auth      include    system-login
-auth     optional   pam_kwallet5.so
account   include    system-login
password  include    system-login
session   include    system-login
-session  optional   pam_kwallet5.so
# End /etc/pam.d/sddm
EOF_PAM

cat > "$PKG/etc/pam.d/sddm-autologin" << "EOF_PAM"
# Begin /etc/pam.d/sddm-autologin
auth      [success=1 default=ignore]  pam_succeed_if.so user = plasma-setup quiet
auth      required   pam_succeed_if.so uid >= 1000 quiet
auth      required   pam_permit.so
account   include    system-login
password  required   pam_deny.so
session   include    system-login
session   optional   pam_env.so  readenv=0 user_readenv=0 conffile=/etc/security/sddm-autologin.conf
-session  optional   pam_kwallet5.so
# End /etc/pam.d/sddm-autologin
EOF_PAM

cat > "$PKG/etc/pam.d/sddm-greeter" << "EOF_PAM"
# Begin /etc/pam.d/sddm-greeter
auth      required   pam_permit.so
account   required   pam_permit.so
password  required   pam_deny.so
session   required   pam_limits.so
session   required   pam_unix.so
session   optional   pam_elogind.so
session   required   pam_turnstile.so
# End /etc/pam.d/sddm-greeter
EOF_PAM
chmod 644 "$PKG/etc/pam.d/sddm" "$PKG/etc/pam.d/sddm-autologin" \
	"$PKG/etc/pam.d/sddm-greeter"

install -vdm755 "$PKG/etc/security/limits.d"
cat > "$PKG/etc/security/limits.d/30-sddm.conf" << "EOF_LIM"
sddm  -  rtprio  1
EOF_LIM
chmod 644 "$PKG/etc/security/limits.d/30-sddm.conf"

cat > "$PKG/etc/security/sddm-autologin.conf" << "EOF_ENV"
XDG_CONFIG_DIRS  DEFAULT=/etc/kdos/autologin-xdg:/etc/xdg:/usr/share/kdos/xdg
EOF_ENV
chmod 644 "$PKG/etc/security/sddm-autologin.conf"

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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D KSCREENLOCKER_PAM_PASSWORD_SERVICE=kde \
	-D KSCREENLOCKER_PAM_FINGERPRINT_SERVICE=kde-fingerprint \
	-D KSCREENLOCKER_PAM_SMARTCARD_SERVICE=disabled \
	-D KSCREENLOCKER_PAM_FACE_SERVICE=disabled \
	-D KSCREENLOCKER_PAM_UNIVERSAL2FACTOR_SERVICE=disabled \
	-D CMAKE_REQUIRE_FIND_PACKAGE_loginctl=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
install -vdm755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/kde" << "EOF_PAM"
# Begin /etc/pam.d/kde
auth      include    system-auth
account   include    system-account
# End /etc/pam.d/kde
EOF_PAM

cat > "$PKG/etc/pam.d/kde-fingerprint" << "EOF_PAM"
# Begin /etc/pam.d/kde-fingerprint
auth      required   pam_fprintd.so
account   include    system-account
# End /etc/pam.d/kde-fingerprint
EOF_PAM
chmod 644 "$PKG/etc/pam.d/kde" "$PKG/etc/pam.d/kde-fingerprint"

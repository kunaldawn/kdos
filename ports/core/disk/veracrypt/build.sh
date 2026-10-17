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

make -C src \
	WITHFUSE3=1 \
	TC_EXTRA_CFLAGS="$CFLAGS" \
	TC_EXTRA_CXXFLAGS="$CXXFLAGS" \
	TC_EXTRA_LFLAGS="$LDFLAGS"

install -Dm755 src/Main/veracrypt "$PKG/usr/bin/veracrypt"
install -Dm755 src/Setup/Linux/mount.veracrypt "$PKG/usr/sbin/mount.veracrypt"
install -Dm644 src/Setup/Linux/veracrypt.xml "$PKG/usr/share/mime/packages/veracrypt.xml"
for s in 16 22 24 32 48 64 128 256; do
	install -Dm644 src/Resources/Icons/VeraCrypt-${s}x${s}.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/veracrypt.png"
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/veracrypt.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=VeraCrypt
GenericName=Encrypted Volumes
Comment=Create and mount VeraCrypt encrypted volumes
TryExec=veracrypt
Exec=veracrypt %f
Icon=veracrypt
Terminal=false
StartupWMClass=veracrypt
Categories=GTK;Security;Utility;Filesystem;
Keywords=encryption;encrypted;volume;container;truecrypt;veracrypt;
MimeType=application/x-veracrypt-volume;application/x-truecrypt-volume;
DESKTOP
chmod 644 "$PKG/usr/share/applications/veracrypt.desktop"

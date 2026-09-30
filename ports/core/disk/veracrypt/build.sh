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

# VeraCrypt's own Makefile, under src/, builds against the system wxWidgets
# through wx-config. WITHFUSE3=1 selects libfuse 3, the only FUSE the tree
# ports; without it the build stops looking for FUSE 2. The assembler
# implementations of the ciphers are built with yasm. Release builds run the
# binary's own cipher test vectors after linking and stop on a failure.
# Mounting runs sudo from inside the program, which asks for the password in
# its own dialog, and then dmsetup, losetup and fusermount3.
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

# The entry is written here: upstream's names an absolute Exec path and no
# window class. GTK takes the Wayland app_id from the program name.
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

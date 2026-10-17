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

autoreconf -f -i
./configure \
	--prefix=/usr \
	--enable-qt \
	--enable-sudo=no \
	--with-ncurses \
	--with-ext2fs \
	--with-jpeg \
	--with-ntfs3g \
	--without-ntfs \
	--without-reiserfs \
	--with-ewf \
	--with-iconv \
	--with-zlib \
	--with-uuid
for have in HAVE_LIBNTFS3G HAVE_LIBEWF_HANDLE_READ_BUFFER_AT_OFFSET \
	HAVE_LIBEWF_HANDLE_WRITE_BUFFER_AT_OFFSET; do
	grep -q "^#define $have 1" config.h || {
		echo "testdisk: $have not configured" >&2
		exit 1
	}
done
grep -q '^qphotorec_LDADD = .*-lQt6Widgets' src/Makefile || {
	echo "testdisk: QPhotoRec is not configured against Qt 6" >&2
	exit 1
}
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/qphotorec.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=QPhotoRec
GenericName=Data Recovery
Comment=Recover deleted files from a disk, a memory card or a disk image
Exec=qphotorec
Icon=qphotorec
Terminal=false
StartupWMClass=qphotorec
Categories=Qt;System;Filesystem;
Keywords=recover;undelete;photorec;testdisk;carve;
DESKTOP
chmod 644 "$PKG/usr/share/applications/qphotorec.desktop"

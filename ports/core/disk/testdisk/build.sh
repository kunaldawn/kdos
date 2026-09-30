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

# THE SOURCE IS A COMMIT ON master, NOT THE 7.2 RELEASE: 7.2 builds QPhotoRec
# against Qt 5 only, master against Qt 6. The tarball is a git archive with no
# configure script, hence autoreconf.
#
# QPhotoRec IS ON AND IT IS Qt 6. configure tries Qt 6 and, when that fails,
# drops to Qt 5 without a word, and to no window at all when Qt 5 or a moc,
# rcc or lrelease is missing too; the check below turns any of those into a
# failed build. It reads qphotorec's own link line, because the Qt 6 library
# variables are set whenever the .pc files are found, window or not. moc and
# rcc are found through Qt6Core.pc's libexecdir, lrelease through its bindir.
#
# EVERY LIBRARY IS NAMED, because configure's default is "use it if found". An
# explicit --with-X makes a missing library a configure error for ext2fs, jpeg,
# ewf, zlib and iconv. ntfs3g's own check tests a misspelt variable and never
# fires, so config.h is checked for it below, and for the two libewf handle
# calls without which there are no E01 forensic images. --without-ntfs and
# --without-reiserfs are the legacy libntfs and progsreiserfs, neither of which
# is a port; ntfs-3g covers NTFS.
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

# Upstream's entry has no StartupWMClass. QPhotoRec sets no desktop file name
# and no organisation domain, so Qt makes its Wayland app_id the program's own
# name, which the entry repeats. The icon is upstream's 48-pixel PNG in
# hicolor. QPhotoRec lists only the disks its user can open: recovering from a
# whole device means starting it as root, and a disk image works as anyone.
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

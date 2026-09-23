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

# The chattr dialog (ext2fs attributes) has no switch: configure turns it on
# whenever ext2fs.pc and e2p.pc are found, which is why e2fsprogs is in depends.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--with-screen=slang \
	--without-x \
	--enable-vfs-sftp=yes \
	--disable-doxygen-doc \
	--enable-nls \
	--disable-aspell \
	--without-gpm-mouse \
	--disable-tests
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/mc.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Files
GenericName=File Manager
Comment=Browse files with Midnight Commander
Exec=mc %f
Icon=folder
Terminal=true
Categories=System;FileTools;FileManager;
MimeType=inode/directory;
Keywords=file;manager;browser;mc;midnight;commander;
EOF
chmod 644 "$PKG/usr/share/applications/mc.desktop"

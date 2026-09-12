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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--with-screen=slang \
	--without-x \
	--enable-vfs-smb=no \
	--enable-vfs-sftp=yes \
	--disable-doxygen-doc
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
Icon=file-manager
Terminal=true
Categories=System;FileTools;FileManager;
MimeType=inode/directory;
Keywords=file;manager;browser;mc;midnight;commander;
EOF
chmod 644 "$PKG/usr/share/applications/mc.desktop"

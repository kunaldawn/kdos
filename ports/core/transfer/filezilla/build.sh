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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-locales \
	--disable-manualupdatecheck \
	--disable-autoupdatecheck \
	--without-dbus \
	--with-pugixml=system \
	--enable-ftp \
	--enable-sftp \
	--disable-storj
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/filezilla.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=FileZilla
GenericName=FTP Client
Comment=Download and upload files over FTP, FTPS and SFTP
Exec=filezilla
Icon=filezilla
Terminal=false
StartupWMClass=filezilla
Categories=Network;FileTransfer;
Keywords=ftp;ftps;sftp;upload;download;transfer;filezilla;
DESKTOP
chmod 644 "$PKG/usr/share/applications/filezilla.desktop"

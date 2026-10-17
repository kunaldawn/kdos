# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/rpath-origin.patch"

export CROSSCFLAGS="-O2 -pipe"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--enable-archs=x86_64,i386 \
	--with-mingw=clang \
	--disable-tests \
	--with-wayland \
	--with-x \
	--with-xcomposite \
	--with-xcursor \
	--with-xfixes \
	--with-xinerama \
	--with-xinput \
	--with-xinput2 \
	--with-xrandr \
	--with-xrender \
	--with-xshape \
	--with-xshm \
	--with-xxf86vm \
	--with-opengl \
	--with-vulkan \
	--with-opencl \
	--with-alsa \
	--with-pulse \
	--with-cups \
	--with-dbus \
	--with-udev \
	--with-usb \
	--with-sdl \
	--with-v4l2 \
	--with-gphoto \
	--with-sane \
	--with-pcap \
	--with-pcsclite \
	--with-gnutls \
	--with-krb5 \
	--with-gssapi \
	--with-gstreamer \
	--with-ffmpeg \
	--with-fontconfig \
	--with-freetype \
	--with-inotify \
	--with-unwind \
	--with-pthread \
	--without-hwloc \
	--without-oss \
	--without-capi \
	--without-coreaudio \
	--without-netapi
make
make DESTDIR=$PKG install

ln -s wine "$PKG/usr/bin/wine64"

find "${PKG:?}/usr/share/man" -mindepth 1 -maxdepth 1 -name '*.UTF-8' -exec rm -rf {} +

install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
rsvg-convert -w 128 -h 128 programs/winecfg/logo.svg \
	-o "$PKG/usr/share/icons/hicolor/128x128/apps/wine.png"
rsvg-convert -w 128 -h 128 programs/winecfg/winecfg.svg \
	-o "$PKG/usr/share/icons/hicolor/128x128/apps/wine-winecfg.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/wine.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Wine Program Loader
Comment=Run a Windows program
Exec=wine start /unix %f
Icon=wine
Terminal=false
NoDisplay=true
MimeType=application/x-msdownload;application/x-msi;application/x-ms-shortcut;application/x-bat;
EOF2
cat > "$PKG/usr/share/applications/wine-winecfg.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Wine Configuration
GenericName=Windows Compatibility Settings
Comment=Windows version, drives, graphics and audio for Windows programs
Exec=winecfg
Icon=wine-winecfg
Terminal=false
StartupWMClass=winecfg.exe
Categories=System;Emulator;
Keywords=wine;windows;exe;compatibility;winecfg;
EOF2
chmod 644 "$PKG"/usr/share/applications/wine*.desktop

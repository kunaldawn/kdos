# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Wine's unix libraries under /usr/lib/wine/x86_64-unix name each other in
# DT_NEEDED by bare file name. musl's loader matches an already-loaded library
# only when it was found by a search, not by the path Wine dlopens it with, so
# without an $ORIGIN run path every one of them fails to load.
patch -p1 -i "$PORT_SRC/rpath-origin.patch"

# WoW64: one 64-bit loader runs both 64- and 32-bit Windows programs, and no
# 32-bit Linux library is needed. The PE half is compiled by clang in its
# MSVC mode and linked by lld-link (--with-mingw=clang); Wine carries its own
# C runtime, so no MinGW toolchain or headers are involved. CROSSCFLAGS
# replaces upstream's "-g -O2", so the DLLs carry no debug information.
#
# Both display drivers are built: winewayland is the default where
# WAYLAND_DISPLAY is set, and winex11 serves a program that needs X11 through
# Xwayland. Wine reads the CPU topology from sysfs; its hwloc path is FreeBSD's.
# OSS, CAPI and CoreAudio are not on this system. The Samba NetAPI library is
# not used. The regression tests are not built.
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

# Scripts written for the old split build call wine64; in WoW64 the one
# loader is both.
ln -s wine "$PKG/usr/bin/wine64"

# English manual pages only.
find "${PKG:?}/usr/share/man" -mindepth 1 -maxdepth 1 -name '*.UTF-8' -exec rm -rf {} +

# The loader opens a Windows program from the file manager; nothing lists it.
# winecfg is the one menu row: a prefix is made on first use, and every other
# Windows program is reached through the file it opens or `wine <program>`.
# winecfg's window is an X11 or Wayland toplevel whose class is its
# executable name, winecfg.exe.
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

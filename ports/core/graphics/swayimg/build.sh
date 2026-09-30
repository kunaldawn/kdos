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

# Every image format is named on, so a missing library fails the setup
# instead of leaving a viewer that silently cannot open that type. video is
# the ffmpeg storyboard of a clip's frames, raw the camera formats through
# libraw, sixel the terminal output.
#
# wayland and drm are both UIs: drm draws on the console with no compositor.
# compositor is the Sway and Hyprland IPC client, which kdos-comp does not
# speak, so it stays off. The markdown docs are off; the manual page, the
# example Lua configuration and both shell completions ship.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dversion=$version \
	-Dwayland=enabled \
	-Ddrm=enabled \
	-Dcompositor=disabled \
	-Dliblua=luajit \
	-Dexr=enabled \
	-Dgif=enabled \
	-Dheif=enabled \
	-Davif=enabled \
	-Djpeg=enabled \
	-Djp2=enabled \
	-Djxl=enabled \
	-Dpng=enabled \
	-Dsvg=enabled \
	-Dtiff=enabled \
	-Dsixel=enabled \
	-Draw=enabled \
	-Dwebp=enabled \
	-Dvideo=enabled \
	-Dexif=enabled \
	-Dbash=enabled \
	-Dzsh=enabled \
	-Ddesktop=true \
	-Dman=true \
	-Dluameta=true \
	-Ddoc=false \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# UPSTREAM'S ENTRY IS REPLACED to add StartupWMClass: the window's app_id is
# "swayimg". It stays NoDisplay, a handler opened on a file like kdos-pix and
# imv, and its hicolor PNGs are upstream's, installed above.
cat > "$PKG/usr/share/applications/swayimg.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Swayimg
GenericName=Image Viewer
Comment=Image viewer for Wayland
TryExec=swayimg
Exec=swayimg %F
Icon=swayimg
Terminal=false
NoDisplay=true
StartupNotify=false
StartupWMClass=swayimg
Categories=Graphics;Viewer;
MimeType=image/avif;image/bmp;image/gif;image/heif;image/jpeg;image/jpg;image/jxl;image/pbm;image/pjpeg;image/png;image/svg+xml;image/tiff;image/webp;image/x-bmp;image/x-exr;image/x-png;image/x-portable-anymap;image/x-portable-bitmap;image/x-portable-graymap;image/x-portable-pixmap;image/x-targa;image/x-tga;
DESKTOP
chmod 644 "$PKG/usr/share/applications/swayimg.desktop"

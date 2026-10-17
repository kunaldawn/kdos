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

mkdir -p build && cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DTIMG_VERSION_FROM_GIT=Off \
	-DWITH_GRAPHICSMAGICK=On \
	-DWITH_STB_IMAGE=On \
	-DWITH_QOI_IMAGE=On \
	-DWITH_TURBOJPEG=On \
	-DWITH_RSVG=On \
	-DWITH_POPPLER=On \
	-DWITH_LIBSIXEL=On \
	-DWITH_VIDEO_DECODING=On \
	-DWITH_VIDEO_DEVICE=On \
	-DWITH_OPENSLIDE_SUPPORT=Off
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/timg.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Image Viewer (timg)
GenericName=Image Viewer
Comment=Pictures and video in a terminal
Exec=timg %F
Icon=image-x-generic
Terminal=true
Categories=Graphics;Viewer;
MimeType=image/png;image/jpeg;image/gif;image/webp;image/bmp;image/tiff;
Keywords=image;picture;photo;view;sixel;kitty;timg;
X-KDOS-Term=kdos-term
EOF
chmod 644 "$PKG/usr/share/applications/timg.desktop"

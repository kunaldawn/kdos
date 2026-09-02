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

# TWO LOADERS ARE OFF AND EACH IS A DEPENDENCY THIS TREE DOES NOT HAVE.
#
# GraphicsMagick is not a port, so the general loader is STB plus QOI and the
# format-specific ones carry the rest: turbojpeg for JPEG, librsvg for SVG and
# ffmpeg for video.
#
# WITH_POPPLER wants poppler-glib and the poppler port builds -DENABLE_GLIB=OFF
# — the glib binding drags cairo and gdk-pixbuf in for a host with no GTK. So
# timg reads pictures and video here, not PDFs; `pdftoppm` and a pipe is the
# route for those.
#
# WITH_TURBOJPEG also requires libexif, which is REQUIRED rather than optional
# in the same branch: it reads the orientation tag, and without it a photo
# taken sideways is displayed sideways.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DTIMG_VERSION_FROM_GIT=Off \
	-DWITH_GRAPHICSMAGICK=Off \
	-DWITH_STB_IMAGE=On \
	-DWITH_QOI_IMAGE=On \
	-DWITH_TURBOJPEG=On \
	-DWITH_RSVG=On \
	-DWITH_POPPLER=Off \
	-DWITH_LIBSIXEL=On \
	-DWITH_VIDEO_DECODING=On \
	-DWITH_VIDEO_DEVICE=Off \
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
EOF
chmod 644 "$PKG/usr/share/applications/timg.desktop"

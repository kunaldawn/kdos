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

# EVERY DECODER AND ENCODER PLUGIN IS REQUIRED, not probed: upstream marks
# each library OPTIONAL and drops the plugin when it is absent, so a missing
# port would ship a K3b that silently cannot burn an MP3 or FLAC as audio CD.
# Musepack is off (libmpcdec is not a port). The SoX and external-program
# encoders call programs at run time and need no library.
#
# Qt6WebEngineWidgets only restyles the disk-info pane, and without it the pane
# is a plain Qt view; it is disabled so a Chromium engine is not pulled in for
# that. KF_SKIP_PO_PROCESSING: bundled data is English only.
#
# Writing runs external programs that K3b finds on $PATH: cdrskin (libburn)
# for CD and DVD, growisofs (dvd+rw-tools) for DVD and BD, cdrdao for
# disc-at-once and copies, and mkisofs for data images. Audio-CD ripping
# dlopen()s libcdio-paranoia. The KAuth helper k3bhelper and its polkit action
# org.kde.k3b.updatepermissions change the burner device's group.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D K3B_DOC=ON \
	-D K3B_DEBUG=OFF \
	-D K3B_BUILD_API_DOCS=OFF \
	-D K3B_ENABLE_DVD_RIPPING=ON \
	-D K3B_ENABLE_TAGLIB=ON \
	-D K3B_BUILD_FFMPEG_DECODER_PLUGIN=ON \
	-D K3B_BUILD_OGGVORBIS_DECODER_PLUGIN=ON \
	-D K3B_BUILD_OGGVORBIS_ENCODER_PLUGIN=ON \
	-D K3B_BUILD_MAD_DECODER_PLUGIN=ON \
	-D K3B_BUILD_MUSE_DECODER_PLUGIN=OFF \
	-D K3B_BUILD_FLAC_DECODER_PLUGIN=ON \
	-D K3B_BUILD_SNDFILE_DECODER_PLUGIN=ON \
	-D K3B_BUILD_LAME_ENCODER_PLUGIN=ON \
	-D K3B_BUILD_SOX_ENCODER_PLUGIN=ON \
	-D K3B_BUILD_EXTERNAL_ENCODER_PLUGIN=ON \
	-D K3B_BUILD_WAVE_DECODER_PLUGIN=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_DvdRead=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Taglib=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FFmpeg=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Flac=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Flac++=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Mad=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Sndfile=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Lame=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OggVorbis=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.k3b, not upstream's "k3b". It
# claims only K3b's own project type: disc images and blank media are types
# other programs open too, and mimeapps.list is where a default is chosen. The
# hicolor PNGs it names are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.k3b.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=K3b
GenericName=Disc Burning
Comment=Burn, copy and rip CDs, DVDs and Blu-ray discs
TryExec=k3b
Exec=k3b %U
Icon=k3b
Terminal=false
StartupNotify=true
StartupWMClass=org.kde.k3b
X-DocPath=k3b/index.html
MimeType=application/x-k3b;
Categories=Qt;KDE;AudioVideo;DiscBurning;
Keywords=burn;cd;dvd;blu-ray;iso;rip;disc;k3b;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.k3b.desktop"

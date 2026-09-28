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


# THE CLOSURE LIVES UNDER ITS OWN PREFIX, /usr/lib/sideband. LXST (the voice
# transport), pycodec2, mistune and audioop-lts are imported by Sideband
# alone, and there they cannot collide with a copy another port puts in
# site-packages. RNS, LXMF, Kivy, numpy, Pillow, qrcode and the rest are
# ports, found in site-packages through depends.
#
# Of Sideband's declared requirements, materialyoucolor and beautifulsoup4
# are imported by no module it ships, and are not installed.
_home=/usr/lib/sideband

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# pycodec2 binds codec2's C library and builds against numpy's headers with
# Cython; audioop-lts is the audioop module python 3.13 removed, which LXST's
# pydub imports. Build isolation is off, so the Cython, numpy and setuptools
# ports are the build backends and nothing is fetched. --ignore-installed
# keeps the prefix whole when a copy is already in the build root.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	pycodec2 mistune audioop-lts

# LXST builds its filter library from LXST/Filters.c, loaded at run time
# through cffi. The source tree also carries prebuilt Windows and macOS
# libraries for its bundled pyogg, which setup.py installs as package data;
# they are code for other systems and are removed. pyogg then loads the
# system's libopus by name at run time, for the Opus voice codec; without
# the opus port, calls have no codec but Codec2.
pip3 install --no-deps --no-index --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	"$SRC_ROOT/LXST-$_lxst"
_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG$_home")
# platlib is built from platbase, not base: naming base alone resolves it to
# the build root's own site-packages, and the rm below would reach outside $PKG.
_plat=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"base": sys.argv[1], "platbase": sys.argv[1]}))' "$PKG$_home")
rm -rf "$_site/LXST/Codecs/libs/pyogg/libs" "$_plat/LXST/Codecs/libs/pyogg/libs"

pip3 install --no-deps --no-index --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home .

# setup.py's data_files land under the prefix; the entry and the icon belong
# in /usr/share, and the entry is written below.
install -Dm644 sbapp/assets/io.unsigned.sideband.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/io.unsigned.sideband.png"
rm -rf "$PKG$_home/share"

# ONE LAUNCHER. It puts the prefix's site directories ahead of any
# PYTHONPATH the caller set, resolved through sysconfig so it names no python
# version, and runs the entry point pip wrote. SDL_APP_ID is the Wayland
# app_id and the X11 class SDL3 gives the window; unset, it is the python
# interpreter's name, which no menu entry can match.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/sideband" <<'KDOS_SH'
#!/bin/sh
home=/usr/lib/sideband
site=$(python3 -c 'import sys, sysconfig; b = sys.argv[1]; print(sysconfig.get_path("purelib", vars={"base": b}) + ":" + sysconfig.get_path("platlib", vars={"platbase": b, "base": b}))' "$home") || exit 1
PYTHONPATH=$site${PYTHONPATH:+:$PYTHONPATH}
SDL_APP_ID=${SDL_APP_ID:-io.unsigned.sideband}
export PYTHONPATH SDL_APP_ID
exec "$home/bin/sideband" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/sideband"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/io.unsigned.sideband.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Sideband
GenericName=Mesh Messaging
Comment=Messages, voice, telemetry and maps over a Reticulum mesh
Exec=sideband
Icon=io.unsigned.sideband
Terminal=false
StartupWMClass=io.unsigned.sideband
Categories=Network;Chat;
Keywords=reticulum;lxmf;mesh;lora;radio;chat;voice;telemetry;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.unsigned.sideband.desktop"

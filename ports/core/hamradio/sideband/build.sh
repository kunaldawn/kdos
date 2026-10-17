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

_home=/usr/lib/sideband

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	pycodec2 mistune audioop-lts

pip3 install --no-deps --no-index --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	"$SRC_ROOT/LXST-$_lxst"
_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG$_home")
_plat=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"base": sys.argv[1], "platbase": sys.argv[1]}))' "$PKG$_home")
rm -rf "$_site/LXST/Codecs/libs/pyogg/libs" "$_plat/LXST/Codecs/libs/pyogg/libs"

pip3 install --no-deps --no-index --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home .

install -Dm644 sbapp/assets/io.unsigned.sideband.png \
	"$PKG/usr/share/icons/hicolor/512x512/apps/io.unsigned.sideband.png"
rm -rf "$PKG$_home/share"

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

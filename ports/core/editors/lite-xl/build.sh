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

patch -p1 -i "$PORT_SRC/cve-2025-12121-remove-legacy-exec-function.patch"
patch -p1 -i "$PORT_SRC/poor-text-rendering-20260312-a17c0455.patch"

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback \
	-Duse_system_lua=true \
	-Ddirmonitor_backend=inotify
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/icons/lite-xl.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/lite-xl.png"
done

cat > "$PKG/usr/share/applications/org.lite_xl.lite_xl.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Lite XL
GenericName=Text Editor
Comment=A lightweight text editor written in Lua
TryExec=lite-xl
Exec=lite-xl %F
Icon=lite-xl
Terminal=false
StartupWMClass=com.lite_xl.LiteXL
Categories=Development;TextEditor;
Keywords=text;editor;code;lua;lite;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.lite_xl.lite_xl.desktop"

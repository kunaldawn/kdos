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

# Alpine's two patches. system.exec handed a string to system(); it is
# removed and its callers start an argument vector instead. The LCD filter is
# left at FreeType's default, since lite-xl's own weights render text poorly.
patch -p1 -i "$PORT_SRC/cve-2025-12121-remove-legacy-exec-function.patch"
patch -p1 -i "$PORT_SRC/poor-text-rendering-20260312-a17c0455.patch"

# Lua, PCRE2 and FreeType are ports; wrap-mode=nofallback turns a missing one
# into a configure error instead of a static copy from subprojects/.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nofallback \
	-Duse_system_lua=true \
	-Ddirmonitor_backend=inotify
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s resources/icons/lite-xl.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/lite-xl.png"
done

# UPSTREAM'S ENTRY IS REPLACED: SDL3 takes the Wayland app_id from the
# metadata main.c sets, com.lite_xl.LiteXL, not from the program name.
# MimeType is left out: inode/directory would make it the folder opener, and
# mimeapps.list is where the default text editor is chosen.
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

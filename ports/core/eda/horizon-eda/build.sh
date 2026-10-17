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

patch -p1 -i "$PORT_SRC/podofo-1.patch"

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

cat > "$PKG/usr/share/applications/org.horizon_eda.HorizonEDA.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Horizon EDA
GenericName=PCB Design
Comment=Schematic capture and PCB layout around a pool of parts
Exec=horizon-eda %U
Icon=org.horizon_eda.HorizonEDA
Terminal=false
Categories=GTK;Development;Engineering;Electronics;
Keywords=pcb;schematic;eda;electronics;layout;gerber;
StartupWMClass=org.horizon_eda.HorizonEDA.pool_prj_mgr
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.horizon_eda.HorizonEDA.desktop"

test -x "$PKG/usr/bin/horizon-eda"
test -x "$PKG/usr/bin/horizon-imp"
test -f "$PKG/usr/share/icons/hicolor/64x64/apps/org.horizon_eda.HorizonEDA.png"
grep -q 'HAVE_SPNAV' build/compile_commands.json

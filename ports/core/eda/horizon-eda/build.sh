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


# The project has no option file: every dependency is found by pkg-config, and
# OpenCASCADE through its CMake package, which is why cmake is a build
# dependency. The 3D-mouse support compiles in only when spnav is found, so
# its absence would pass silently; the check at the end catches that.
# The Python module, the PR-review tool and the tests are not built by default.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# GTK takes the Wayland app_id from the GApplication id, and the project
# manager's is org.horizon_eda.HorizonEDA.pool_prj_mgr. The parts pool is
# downloaded by the program itself on first use, so offline it opens with
# no pool until one is copied in.
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

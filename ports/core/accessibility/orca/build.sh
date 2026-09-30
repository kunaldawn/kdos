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

# Orca is Python over GObject introspection: at-spi2-core's Atspi and Atk
# typelibs and its gi/overrides/Atspi.py, and GTK 3's typelib for the
# preferences window. Setup refuses to continue without the override, the
# dasbus module or a GTK 3.24 that imports, so each is in depends. Speech
# goes through the speechd Python module, which speech-dispatcher installs
# only with its Python bindings on; without it Orca starts and says nothing.
# Braille goes through the brlapi and louis Python modules, which brltty and
# liblouis install only with their Python bindings on; without them Orca
# reports braille unavailable and speech still works. Sound cues import
# GStreamer's typelib and are skipped when it is absent.
# MathCAT is off: it is a Rust subproject built by cargo during setup, which
# the offline build cannot fetch. Spiel is off: it is experimental and a meson
# subproject. Mouse review imports Wnck, which is X11-only and not a port, so
# that one feature reports itself unavailable.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dmathcat=false \
	-Dspiel=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# No systemd: the user unit has no manager here to start it.
rm -rf "$PKG/usr/lib/systemd"

# Bundled data is English only: GTK falls back to the source strings, and the
# help keeps its C pages.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED: upstream hides it, and the screen reader is
# something a person must be able to find and start. The preferences window
# is a GTK 3 window whose app_id is the program name. The menu icon is the
# 48x48 hicolor PNG upstream installs as orca.png.
cat > "$PKG/usr/share/applications/orca.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Orca Screen Reader
GenericName=Screen Reader
Comment=Read the screen aloud and in braille
TryExec=orca
Exec=orca --replace
Icon=orca
Terminal=false
StartupWMClass=orca
Categories=GTK;Utility;Accessibility;
Keywords=screen reader;speech;braille;blind;accessibility;a11y;orca;
Actions=preferences;

[Desktop Action preferences]
Name=Preferences
Exec=orca --setup
DESKTOP
chmod 644 "$PKG/usr/share/applications/orca.desktop"

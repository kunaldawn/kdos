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

# The makefile assigns CFLAGS and LDFLAGS itself and never reads them from the
# environment; CPPFLAGS it does read, at the head of its own CFLAGS, so the
# tree's flags travel there. LDFLAGS is empty in the makefile on Linux, so the
# tree's value passed on the command line replaces nothing. OPTFLAGS and
# MAGIC_CFLAGS are the makefile's -O2 -g and -g3 for the program and the magic
# plugins; both are replaced, keeping upstream's $(FASTMATH), so neither ships
# DWARF and the plugins build with the tree's flags. The makefile is not safe
# in parallel.
# PACKAGE_ONLY installs the desktop entries and hicolor icons into DESTDIR
# instead of registering them with xdg-utils on the build machine.
export CPPFLAGS="$CFLAGS -D_POSIX_PRIORITY_SCHEDULING -Wno-implicit-function-declaration"
_make=(
	PREFIX=/usr
	LDFLAGS="$LDFLAGS"
	OPTFLAGS='$(FASTMATH)'
	MAGIC_CFLAGS="$CFLAGS"' $(FASTMATH) -fno-common $(MAGIC_SDL_CPPFLAGS) -Isrc/'
)
make -j1 "${_make[@]}"
make -j1 "${_make[@]}" PACKAGE_ONLY=yes DESTDIR=$PKG install

# Bundled data is English only: the interface catalogues go, and so do the
# handbooks and manual pages in other languages.
rm -rf "$PKG/usr/share/locale"
for d in "$PKG"/usr/share/doc/tuxpaint-$version/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/tuxpaint-$version/en | */tuxpaint-$version/html | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

# The entries are replaced for StartupWMClass. Tux Paint asks for the class
# TuxPaint.TuxPaint by a putenv() of SDL 2's SDL_VIDEO_X11_WMCLASS in main(),
# but sdl2-compat renames that variable to SDL_APP_ID only in its load-time
# constructor, before main() runs, so SDL 3 falls back to the executable name
# and the app_id is tuxpaint.
rm -f "$PKG"/usr/share/applications/tuxpaint*.desktop
cat > "$PKG/usr/share/applications/tuxpaint.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Tux Paint
GenericName=Drawing Program
Comment=A drawing program for children
TryExec=tuxpaint
Exec=tuxpaint
Icon=tuxpaint
Terminal=false
StartupWMClass=tuxpaint
Categories=Education;Art;Graphics;2DGraphics;Game;KidsGame;
Keywords=paint;draw;drawing;children;kids;art;stamps;tuxpaint;
DESKTOP
cat > "$PKG/usr/share/applications/tuxpaint-fullscreen.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Tux Paint (Fullscreen)
GenericName=Drawing Program
Comment=A drawing program for children, filling the screen
TryExec=tuxpaint
Exec=tuxpaint --fullscreen=native
Icon=tuxpaint
Terminal=false
StartupWMClass=tuxpaint
Categories=Education;Art;Graphics;2DGraphics;Game;KidsGame;
Keywords=paint;draw;drawing;children;kids;art;fullscreen;tuxpaint;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/tuxpaint*.desktop

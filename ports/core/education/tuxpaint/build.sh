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

export CPPFLAGS="$CFLAGS -D_POSIX_PRIORITY_SCHEDULING -DHAVE_STRCASESTR -Wno-implicit-function-declaration"
_make=(
	PREFIX=/usr
	LDFLAGS="$LDFLAGS"
	OPTFLAGS='$(FASTMATH)'
	MAGIC_CFLAGS="$CFLAGS"' $(FASTMATH) -fno-common $(MAGIC_SDL_CPPFLAGS) -Isrc/'
)
make -j1 "${_make[@]}"
make -j1 "${_make[@]}" PACKAGE_ONLY=yes DESTDIR=$PKG \
	COMPLETIONDIR=$PKG/usr/share/bash-completion/completions install
mv "$PKG/usr/share/bash-completion/completions/010_tuxpaint-completion.bash" \
	"$PKG/usr/share/bash-completion/completions/tuxpaint"

rm -f "$PKG/usr/share/tuxpaint/fonts/locale/zh_tw_docs/maketuxfont.py"
for d in "$PKG"/usr/share/doc/tuxpaint-$version/*/ "$PKG"/usr/share/man/*/; do
	case ${d%/} in
	*/tuxpaint-$version/en | */tuxpaint-$version/html | */man/man[0-9]*) ;;
	*) rm -rf "$d" ;;
	esac
done

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

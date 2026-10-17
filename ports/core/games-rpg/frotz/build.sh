# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="$CFLAGS -Wno-error"
export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
make curses PREFIX=/usr SOUND_TYPE=none
make install PREFIX=/usr SOUND_TYPE=none DESTDIR=$PKG

install -Dm644 /dev/stdin \
	"$PKG/usr/share/mime/packages/kdos-zmachine.xml" <<'MIMEXML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-zmachine">
    <comment>Z-machine story file</comment>
    <glob pattern="*.z1"/>
    <glob pattern="*.z2"/>
    <glob pattern="*.z3"/>
    <glob pattern="*.z4"/>
    <glob pattern="*.z5"/>
    <glob pattern="*.z6"/>
    <glob pattern="*.z7"/>
    <glob pattern="*.z8"/>
  </mime-type>
  <mime-type type="application/x-blorb">
    <comment>Blorb resource file</comment>
    <glob pattern="*.zblorb"/>
    <glob pattern="*.zlb"/>
    <glob pattern="*.blorb"/>
    <glob pattern="*.blb"/>
  </mime-type>
</mime-info>
MIMEXML

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/$name.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Frotz
GenericName=Interactive Fiction
Comment=Play a Z-machine text adventure
Exec=frotz %f
Icon=input-gaming
Terminal=true
Categories=Game;AdventureGame;
MimeType=application/x-zmachine;application/x-blorb;
Keywords=text;adventure;interactive;fiction;zmachine;infocom;story;frotz;
ENTRY
chmod 644 "$PKG/usr/share/applications/$name.desktop"

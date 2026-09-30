# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The curses interface and no other. The SDL one wants a window server and the
# dumb one has no screen model; this desktop is a character grid either way.
#
# SOUND_TYPE=none because the alternative is libao or OSS: an interpreter that
# pulled in an audio stack for the handful of stories that use sound would put
# it on every image for a feature almost nothing exercises.
#
# -Wno-error because upstream's warnings meet this tree's -Werror, and a flag
# is the answer where a patch is not needed.
export CFLAGS="$CFLAGS -Wno-error"
make curses PREFIX=/usr SOUND_TYPE=none
make install PREFIX=/usr SOUND_TYPE=none DESTDIR=$PKG

# THE TYPE HAS TO EXIST BEFORE AN ENTRY CAN CLAIM IT. shared-mime-info 2.5.1 —
# the version this image builds — defines no z-machine type at all, so a
# `MimeType=application/x-zmachine;` line with nothing behind it resolves to
# nothing and says so nowhere. The opener chain keys off /usr/share/mime/globs,
# which kpkg regenerates from this directory when the package is installed.
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

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

# A PROGRAM THAT OPENS NOTHING IS ONE NOBODY OPENS TWICE. `frotz` with no story
# prints usage and exits, so the entry names one and the package carries it.
# The second `source` is not a tarball, so kpkg copies it into $SRC under its
# own name rather than unpacking it — which is why this is a bare relative
# name.
install -Dm644 "$_story" "$PKG/usr/share/$name/$_story"

# MIT REQUIRES THE NOTICE TO TRAVEL WITH THE WORK, so the story's licence ships
# beside it. The text is the author's own, from the published Inform source.
install -Dm644 /dev/stdin "$PKG/usr/share/licenses/$name/$_story.MIT" <<'LICENCE'
Gnu in the Zoo

Copyright (c) 2010 Alex Ball <a.j.ball@bath.ac.uk>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
LICENCE

# THE TYPE HAS TO EXIST BEFORE AN ENTRY CAN CLAIM IT. shared-mime-info 1.10 —
# the version this image builds — defines no z-machine type at all, so a
# `MimeType=application/x-zmachine;` line with nothing behind it resolves to
# nothing and says so nowhere. The opener chain keys off /usr/share/mime/globs,
# which is generated from this directory by the hook in postinstall.sh.
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
Exec=frotz /usr/share/frotz/Gnu_in_the_Zoo.zblorb
Icon=input-gaming
Terminal=true
Categories=Game;AdventureGame;
MimeType=application/x-zmachine;application/x-blorb;
Keywords=text;adventure;interactive;fiction;zmachine;infocom;story;frotz;
ENTRY
chmod 644 "$PKG/usr/share/applications/$name.desktop"

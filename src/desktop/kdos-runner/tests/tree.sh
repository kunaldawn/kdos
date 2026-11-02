#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   tree.sh DIR — a home and a system root for kdos-runner's tests
#
#   DIR/home     $HOME: favorites with codes, a user menu.conf, three box
#                profiles, a kglobalshortcutsrc, a Zeal docset directory
#   DIR/root     $KDOS_RUNNER_ROOT: /etc/kdos/menu.conf, applications,
#                /usr/share/kglobalaccel, two installed KCMs, both books,
#                kdos manual pages, a kiwix library file
#   DIR/bin      stubs first on $PATH: each records its argument vector, one
#                element per line, in DIR/ran/<name>, and `kdos` fails for
#                `kdos theme broken` the way a refused verb does
#
# The user's menu.conf carries routes whose words are shell syntax: the
# tests assert that each arrives at the stub as one literal element.
# ---------------------------------
set -eu
D=$1
rm -rf "$D"
H=$D/home R=$D/root
mkdir -p "$H/.config/kdos/boxes" "$H/.local/share/applications" \
	"$R/etc/kdos" "$R/usr/share/applications" "$R/usr/share/kglobalaccel" \
	"$R/usr/lib/qt6/plugins/plasma/kcms/systemsettings" \
	"$R/usr/share/doc/kdos/02-user-guide" "$R/usr/share/doc/kfs/41-system" \
	"$R/usr/share/man/man1" "$R/usr/share/man/man7" "$R/var/lib/kiwix" \
	"$R/mnt/iso/packs" "$D/bin" "$D/ran"

app() {
	printf '[Desktop Entry]\nType=Application\nName=%s\nIcon=%s\nExec=%s\n%s' \
		"$2" "$3" "$1" "${4:-}" > "$R/usr/share/applications/$1.desktop"
}
app firefox-esr "Firefox ESR" firefox-esr
app org.kde.konsole Konsole utilities-terminal
app org.kde.dolphin Dolphin system-file-manager
app org.kde.kate Kate kate
# A user's copy that hides the system's: its code must not answer.
app gimp GIMP gimp
printf '[Desktop Entry]\nType=Application\nName=GIMP\nHidden=true\n' \
	> "$H/.local/share/applications/gimp.desktop"

cat > "$H/.config/kdos/favorites" <<'EOF'
# a comment line
org.kde.konsole code=TE
org.kde.dolphin code=fm   files
firefox-esr code=FF
gimp code=IM
not-installed code=NI
www
org.kde.kate code=TOOLONG
EOF

cat > "$R/etc/kdos/menu.conf" <<'EOF'
# system routes
@toplevel = Network Sound
setup.network = systemsettings kcm_networkmanagement
setup.sound   = systemsettings kcm_pulseaudio
style.theme   = systemsettings kcm_lookandfeel
bad name      = should-not-appear
EOF
{
	echo 'setup.sound = pavucontrol-qt'
	echo 'fail.theme = kdos theme broken'
	echo "evil.semicolon = kdos why ;touch\${IFS}$D/pwned"
	echo "evil.subst     = kdos why \$(touch $D/pwned) \`touch $D/pwned\`"
} > "$H/.config/kdos/menu.conf"

for b in www dev firefox-esr; do
	printf 'base = debian\n' > "$H/.config/kdos/boxes/$b.conf"
done
printf 'x\n' > "$H/.config/kdos/boxes/not-a-profile.txt"

cat > "$H/.config/kglobalshortcutsrc" <<'EOF'
[kwin]
_k_friendly_name=KWin
Window Maximize=Meta+PgUp,Meta+PgUp,Maximize Window
Toggle Tiles Editor=Meta+T,Meta+T,Toggle Tiles Editor
KrohnkiteToggle=Meta+Shift+T\tMeta+Alt+T,none,Krohnkite: Toggle tiling
Window Close=Alt+F4,Alt+F4,Close Window

[org.kde.krunner.desktop]
_launch=Meta+Space\tAlt+Space\tAlt+F2\tSearch,Alt+Space\tAlt+F2\tSearch,KRunner

[services][org.kde.konsole.desktop]
_launch=Ctrl+Alt+T
EOF
cat > "$R/usr/share/kglobalaccel/org.kde.spectacle.desktop" <<'EOF'
[Desktop Entry]
Name=Spectacle
Actions=RectangularRegionScreenShot;FullScreenScreenShot;

[Desktop Action RectangularRegionScreenShot]
Name=Capture Rectangular Region
X-KDE-Shortcuts=Meta+Shift+Print,Meta+Shift+S

[Desktop Action FullScreenScreenShot]
Name=Capture Entire Desktop
X-KDE-Shortcuts=Shift+Print
EOF

: > "$R/usr/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_kdos_system.so"
: > "$R/usr/lib/qt6/plugins/plasma/kcms/systemsettings/kcm_kdos_firewall.so"

cat > "$R/usr/share/doc/kdos/02-user-guide/updates.html" <<'EOF'
<!doctype html><html><head><title>Updates</title></head><body>
<h1 id="updating-kdos">Updating KDOS</h1>
<p>text</p>
<h2 id="using-the-previous-version">Using the previous version</h2>
<h3 id="photosynthesis-of-slots"><span class="n">3.1</span> Photosynthesis of slots &amp; trees</h3>
<h4 id="deep">Too deep to index</h4>
</body></html>
EOF
cat > "$R/usr/share/doc/kfs/41-system/kdos-box.html" <<'EOF'
<html><body><h1 id="kdos-box" data-number="41.3">kdos-box</h1>
<h2 id="flags">Flags</h2></body></html>
EOF
: > "$R/usr/share/man/man1/kdos-box.1.gz"
: > "$R/usr/share/man/man1/kdos-runner.1"
: > "$R/usr/share/man/man1/ls.1.gz"
: > "$R/usr/share/man/man7/kdos.7.gz"

# The boot medium: two packs, a set, and files that are neither.
: > "$R/mnt/iso/packs/app.gimp.kpack"
: > "$R/mnt/iso/packs/data.tess.kpack"
: > "$R/mnt/iso/packs/office.ktar"
: > "$R/mnt/iso/packs/README"
: > "$R/mnt/iso/packs/bad name.kpack"

cat > "$R/var/lib/kiwix/library.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8" ?>
<library version="20110515">
  <book id="0b1" path="/srv/zim/wikipedia_en_all maxi_2024-01.zim" title="Wikipedia &amp; friends" />
  <book id="0b2" path="wiktionary+fr.zim" />
</library>
EOF

# A Dash docset and a Core Data one, when python can write SQLite.
Z=$H/.local/share/Zeal/Zeal/docsets
mkdir -p "$Z/Python_3.docset/Contents/Resources" "$Z/Old.docset/Contents/Resources"
cat > "$Z/Python_3.docset/Contents/Info.plist" <<'EOF'
<plist><dict><key>CFBundleName</key><string>Python 3</string>
<key>DocSetPlatformFamily</key><string>python</string></dict></plist>
EOF
python3 -I - "$Z" <<'EOF' 2>/dev/null || true
import sqlite3, sys
z = sys.argv[1]
d = sqlite3.connect(z + "/Python_3.docset/Contents/Resources/docSet.dsidx")
d.execute("CREATE TABLE searchIndex(id INTEGER PRIMARY KEY, name TEXT, type TEXT, path TEXT)")
d.executemany("INSERT INTO searchIndex(name, type, path) VALUES (?, ?, ?)",
              [("os.path.join", "Function", "a"), ("os.path", "Module", "b"),
               ("os_path_like%", "Guide", "c"), ("zipfile", "Module", "d")])
d.commit()
d = sqlite3.connect(z + "/Old.docset/Contents/Resources/docSet.dsidx")
d.execute("CREATE TABLE ZTOKENTYPE(Z_PK INTEGER PRIMARY KEY, ZTYPENAME TEXT)")
d.execute("CREATE TABLE ZTOKEN(Z_PK INTEGER PRIMARY KEY, ZTOKENNAME TEXT, ZTOKENTYPE INTEGER)")
d.execute("INSERT INTO ZTOKENTYPE VALUES (1, 'cl')")
d.execute("INSERT INTO ZTOKEN VALUES (1, 'os.pathology', 1)")
d.commit()
EOF

for p in kdos kdos-box kstart systemsettings plasma-discover konsole khelpcenter xdg-open zeal pavucontrol-qt; do
	cat > "$D/bin/$p" <<EOF
#!/bin/sh
: > "$D/ran/$p.tmp"
for a in "\$@"; do printf '%s\n' "\$a" >> "$D/ran/$p.tmp"; done
[ -n "\${XDG_ACTIVATION_TOKEN:-}" ] && printf 'token=%s\n' "\$XDG_ACTIVATION_TOKEN" > "$D/ran/$p.token"
mv "$D/ran/$p.tmp" "$D/ran/$p"
[ "\${1:-}" = theme ] && [ "\${2:-}" = broken ] && { echo "kdos theme: broken is not an accent" >&2; exit 1; }
exit 0
EOF
	chmod +x "$D/bin/$p"
done

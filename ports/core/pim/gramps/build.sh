# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man1' -exec rm -rf {} +

cat > "$PKG/usr/share/applications/org.gramps_project.Gramps.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Gramps
GenericName=Genealogy
Comment=Family trees, people, places and sources, with reports and charts
Exec=gramps %F
Icon=org.gramps_project.Gramps
Terminal=false
StartupWMClass=org.gramps_project.Gramps
Categories=GTK;Office;
MimeType=application/x-gramps;application/x-gedcom;application/x-gramps-package;application/x-gramps-xml;
Keywords=genealogy;family history;family tree;ancestry;gedcom;research;gramps;
EOF2
chmod 644 "$PKG/usr/share/applications/org.gramps_project.Gramps.desktop"

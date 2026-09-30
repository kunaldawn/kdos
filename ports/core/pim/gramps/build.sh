# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# setup.py's build step compiles the catalogues, the manual page and the
# desktop and MIME files with msgfmt, then installs them as data files.
# --no-build-isolation: setuptools and wheel are installed ports.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

# English only: the catalogues and the translated manual pages go. The C
# manual page is share/man/man1/gramps.1.gz.
find "$PKG/usr/share/locale" -mindepth 1 -maxdepth 1 ! -name 'en*' -exec rm -rf {} +
find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man1' -exec rm -rf {} +

# The window's app_id is the GtkApplication id, org.gramps_project.Gramps.
# The Geography view needs osm-gps-map and map tiles from the network; it is
# absent, and Gramps says so in its plugin list rather than failing.
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

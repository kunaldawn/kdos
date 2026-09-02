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

./autogen.sh

# ITS DATABASE IS TWO PLAIN TEXT FILES, which is the property that matters on a
# machine that may be reinstalled from a stick. `~/.local/share/calcurse/apts`
# and `todo` are readable, greppable, diffable and syncable with the rest of
# `$HOME` — no sqlite to corrupt, no export step, and a backup is a copy.
#
# It also speaks iCalendar both directions, so an appointment from somebody
# else's calendar comes in and one going out is a file you can hand over.
# asciidoc IS REQUIRED HERE, not optional: this is a tag archive, so `doc/`
# carries the .txt sources and none of the generated output a release tarball
# would. --without-asciidoc leaves the Makefile still declaring calcurse.1 as a
# prerequisite of all-am, and the build dies on `No rule to make target` after
# every binary has already compiled.
# a2x drives xsltproc against asciidoc's own manpage.xsl, which IMPORTS the
# standard docbook-xsl stylesheet by URL — resolved only through the XML
# catalog. Without this the import silently yields nothing and xsltproc exits 5.
export XML_CATALOG_FILES=/etc/xml/catalog

./configure --prefix=/usr --sysconfdir=/etc
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/calcurse.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Calendar
GenericName=Calendar and Todo
Comment=Appointments, todos and reminders
Exec=calcurse
Icon=x-office-calendar
Terminal=true
Categories=Office;Calendar;
Keywords=calendar;appointment;todo;reminder;calcurse;
EOF
chmod 644 "$PKG/usr/share/applications/calcurse.desktop"

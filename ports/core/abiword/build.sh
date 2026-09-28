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


# The release is a git archive with no generated configure. autogen-common.sh
# writes the plugin lists configure.ac includes, and autoreconf generates the
# rest. The first two patches move the spell checker to Enchant 2's
# pkg-config name and API; the third fixes a NULL cast musl's headers reject.
patch -p1 -i "$PORT_SRC/enchant-2-pkgconfig.patch"
patch -p1 -i "$PORT_SRC/enchant.patch"
patch -p1 -i "$PORT_SRC/musl-1.2.3.patch"
./autogen-common.sh
autoreconf -fi

# The plugin list is explicit, so a missing library fails the configure
# instead of dropping a file format. Left out: collab, google, wikipedia,
# urldict and gdict, which only reach network services; grammar, aiksaurus,
# mathview, ots, gda, psion and wmf, whose libraries have no port; and garble,
# a tool for sending documents with the text scrambled. libical, Evolution
# Data Server, Redland and libchamplain have no port either.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-spell \
	--enable-print \
	--enable-plugins="applix bmp clarisworks command docbook eml epub gimp goffice hancom hrtext iscii kword latex loadbindings mht mif mswrite opendocument openwriter openxml opml paint passepartout pdb pdf presentation rsvg s5 sdw t602 wml wordperfect wpg xslfo" \
	--with-goffice \
	--with-gio \
	--without-gnomevfs \
	--without-redland \
	--without-evolution-data-server \
	--without-libical \
	--without-champlain \
	--disable-introspection
make
make DESTDIR=$PKG install

# UPSTREAM'S ENTRY IS REPLACED with a MimeType naming AbiWord's own format
# only: LibreOffice is the word processor that claims the shared office types
# (Word, OpenDocument, RTF, WordPerfect), and a second claim would open them in
# whichever entry sorted first.
cat > "$PKG/usr/share/applications/abiword.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=AbiWord
GenericName=Word Processor
Comment=Compose, edit and view documents
TryExec=abiword
Exec=abiword %U
Icon=abiword
Terminal=false
StartupNotify=true
Categories=Office;WordProcessor;GTK;
MimeType=application/x-abiword;
Keywords=word;processor;document;doc;rtf;odt;abiword;
DESKTOP
chmod 644 "$PKG/usr/share/applications/abiword.desktop"

# Every interface translation is a .strings file installed whatever configure
# is told, so all but the English ones are removed from the package: bundled
# data is English only.
find "$PKG/usr/share/abiword-${version%.*}/strings" -name '*.strings' ! -name 'en*' -delete

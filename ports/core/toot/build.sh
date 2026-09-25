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

# THE CLOSURE IS IN THE BUNDLE, except where a module is already a port.
# `pypackages` names the part of toot's runtime set that no other program here
# imports: beautifulsoup4, soupsieve and pysocks. click, python-dateutil,
# requests (with certifi, charset-normalizer, idna and urllib3), tomlkit,
# typing-extensions (which beautifulsoup4 imports), urwid and wcwidth come from
# their python3-* ports: a module two packages each
# installed would be a path both own, and which of them the image kept would
# follow build order.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF, so each sdist builds with the backend already
# installed: hatchling for beautifulsoup4 and soupsieve, setuptools for pysocks,
# and setuptools-scm for toot itself — all ports in `depends`.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	beautifulsoup4 soupsieve pysocks .

# `toot tui` AND NOT `toot`. The bare command prints its usage and exits, so a
# menu row on it would open a terminal to show a help page and close — the TUI
# is the screen a person means when they click a row.
#
# THE IMAGE EXTRA IS NOT INSTALLED. `pillow` and `term-image` are what draw a
# picture in the timeline, and neither is here: the console draws pictures
# through kdos-term's own protocol and a second decoder would be a second
# answer to the same question.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/toot.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Fediverse
GenericName=Mastodon Client
Comment=Read and post to the Fediverse
Exec=toot tui
Icon=application-rss+xml
Terminal=true
Categories=Network;
Keywords=mastodon;fediverse;activitypub;social;toot;
EOF2
chmod 644 "$PKG/usr/share/applications/toot.desktop"

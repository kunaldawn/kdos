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

# THE CLOSURE IS IN THE BUNDLE. `pypackages` names toot's whole runtime set
# and the build backends four of those tarballs ask for — `python-dateutil`
# pins `setuptools_scm<8.0` while `urwid` wants `>=8`, so pip cannot resolve
# the two in one batch and the backends have to be named rather than crawled.
# `--no-build-isolation` is what makes that legal: each package builds against
# what is already installed rather than against its own declared range.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Backends into the build root, not into $PKG, and in bootstrap order — see
# ports/core/khard/build.sh, which does the same for the same reason.
pyb() { pip3 install --no-index --find-links=vendor --no-build-isolation "$@"; }
pyb packaging
pyb setuptools-scm
pyb flit-core
pyb poetry-core
pyb hatchling
pyb hatch-vcs

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	beautifulsoup4 soupsieve click python-dateutil six pysocks tomlkit \
	urwid wcwidth requests certifi charset-normalizer idna urllib3 .

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
Icon=internet-news-reader
Terminal=true
Categories=Network;
Keywords=mastodon;fediverse;activitypub;social;toot;
EOF2
chmod 644 "$PKG/usr/share/applications/toot.desktop"

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

# ONE bash SCRIPT OVER A git REPOSITORY OF PLAIN FILES, and that is the whole
# design. Every note is a markdown file, every change is a commit, and the
# history is `git log` — so the notes outlive nb itself and sync with anything
# that syncs a directory. On a machine whose premise is that the software may
# be reinstalled from a stick, a notes app with its own database is the wrong
# shape.
#
# `pandoc`, `w3m` and `pdftotext` extend it (import a page, read a bookmark
# offline) and none is required — nb reports what a feature needs rather than
# failing, so they are not in depends.
install -Dm755 nb $PKG/usr/bin/nb
install -Dm644 etc/nb-completion.bash $PKG/usr/share/bash-completion/completions/nb

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/nb.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Notes
GenericName=Note Taking
Comment=Notes, bookmarks and a searchable archive
Exec=nb
Icon=x-office-document
Terminal=true
Categories=Office;Utility;
Keywords=note;bookmark;archive;wiki;nb;
EOF
chmod 644 "$PKG/usr/share/applications/nb.desktop"

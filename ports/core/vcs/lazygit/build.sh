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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export CGO_ENABLED=0
go build -ldflags "-s -w -X main.version=$version" -o lazygit
install -Dm755 lazygit $PKG/usr/bin/lazygit

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/lazygit.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Git
GenericName=Version Control
Comment=Stage, commit, branch and merge
Exec=lazygit
Icon=vcs-normal
Terminal=true
Categories=Development;RevisionControl;
Keywords=git;commit;branch;merge;diff;lazygit;
EOF
chmod 644 "$PKG/usr/share/applications/lazygit.desktop"

install -d "$PKG/usr/share/kio/servicemenus"
cat > "$PKG/usr/share/kio/servicemenus/lazygit.desktop" <<'EOF'
[Desktop Entry]
Type=Service
MimeType=inode/directory;
Actions=gitstatus;
X-KDE-Priority=TopLevel

[Desktop Action gitstatus]
Name=Git Status Here
Icon=vcs-normal
Exec=konsole --workdir %f -e lazygit
EOF
chmod 644 "$PKG/usr/share/kio/servicemenus/lazygit.desktop"

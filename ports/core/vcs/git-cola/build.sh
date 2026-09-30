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

# git-cola runs on PyQt6 through QtPy; polib, which reads its translation
# catalogues, is the one runtime module that is not a port and comes from the
# vendor bundle. The copies of polib and QtPy under extras/ are for upstream's
# Windows installer and are not installed.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
export SETUPTOOLS_SCM_PRETEND_VERSION=$version
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr polib .

# The manual pages, from the Sphinx sources; sphinxtogithub, an extension
# conf.py loads, is under extras/ and on its path.
(cd docs && sphinx-build -b man -q -d "$SRC_ROOT/cola-doctrees" . "$SRC_ROOT/cola-man")
install -Dm644 "$SRC_ROOT"/cola-man/git-cola.1 "$SRC_ROOT"/cola-man/git-dag.1 \
	-t "$PKG/usr/share/man/man1"

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s cola/icons/git-cola.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/git-cola.png"
done

# Upstream's entries are installed by its Makefile, not by the Python
# package, so they are written here. Both programs set the desktop file name,
# and so the Wayland app_id, to git-cola. The folder handler of upstream's
# set is left out: it would claim inode/directory, the file manager's type.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/git-cola.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Git Cola
GenericName=Git Client
Comment=Stage, commit, branch and review changes in a Git repository
TryExec=git-cola
Exec=git-cola --prompt
Icon=git-cola
Terminal=false
StartupNotify=true
StartupWMClass=git-cola
Categories=Development;RevisionControl;
Keywords=git;commit;stage;diff;branch;vcs;cola;
DESKTOP
cat > "$PKG/usr/share/applications/git-dag.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Git DAG
GenericName=Git History Viewer
Comment=Browse a Git repository's history as a graph
TryExec=git-dag
Exec=git-dag --prompt
Icon=git-cola
Terminal=false
StartupNotify=true
StartupWMClass=git-cola
Categories=Development;RevisionControl;
Keywords=git;history;log;graph;dag;branch;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/git-cola.desktop \
	"$PKG"/usr/share/applications/git-dag.desktop

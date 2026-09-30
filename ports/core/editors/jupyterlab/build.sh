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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# Two build backends in the bundle are not ports. They are installed into a
# directory of their own on PYTHONPATH for this build only, so nothing outside
# the package is written.
_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" \
	hatch-fancy-pypi-readme pdm-backend
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

# The web front ends ship built in the release tarballs (jupyterlab/static,
# notebook/static, jupyter_server/static); HATCH_JUPYTER_BUILDER_SKIP_NPM
# keeps every build hook from running npm, and the hooks' ensured targets
# still fail the build if a built file is missing. The argon2 binding links
# the argon2 port instead of compiling its bundled copy.
export HATCH_JUPYTER_BUILDER_SKIP_NPM=1
export ARGON2_CFFI_USE_SYSTEM=1
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	anyio argon2-cffi argon2-cffi-bindings arrow async-lru babel fqdn h11 \
	httpcore httpx isoduration json5 jsonpointer jupyter-builder \
	jupyter-events jupyter-lsp jupyter-server jupyter-server-terminals \
	jupyterlab-server notebook notebook-shim prometheus-client \
	python-json-logger rfc3339-validator rfc3986-validator rfc3987-syntax \
	terminado uri-template webcolors websocket-client .

test -e "$PKG"/usr/share/jupyter/lab/static/package.json
test -x "$PKG"/usr/bin/jupyter-lab
test -x "$PKG"/usr/bin/jupyter-notebook

# Offline defaults: the extension manager installs from PyPI and is made
# read-only; the announcements feed and the update check are fetched from
# the network and are turned off.
install -Dm644 /dev/stdin \
	"$PKG/usr/etc/jupyter/jupyter_server_config.d/kdos-offline.json" <<'JSON'
{
  "LabApp": {
    "extension_manager": "readonly",
    "news_url": null,
    "check_for_updates_class": "jupyterlab.NeverCheckForUpdate"
  }
}
JSON

# THE MENU ICONS: upstream ships both as SVG only, which the panel never reads.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	for i in jupyterlab notebook; do
		rsvg-convert -w $s -h $s \
			"$PKG/usr/share/icons/hicolor/scalable/apps/$i.svg" \
			-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$i.png"
	done
done

# Both upstream entries start a server in a terminal and open it in the web
# browser. The Notebook entry is replaced so that only JupyterLab claims
# notebook files.
cat > "$PKG/usr/share/applications/jupyter-notebook.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Jupyter Notebook
GenericName=Computational Notebook
Comment=Run the classic Jupyter Notebook interface in the web browser
Exec=jupyter-notebook
Icon=notebook
Terminal=true
Categories=Development;Education;Science;
Keywords=python;notebook;ipynb;jupyter;
DESKTOP
chmod 644 "$PKG/usr/share/applications/jupyter-notebook.desktop"
test -e "$PKG/usr/share/applications/jupyterlab.desktop"

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

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" \
	hatch-fancy-pypi-readme pdm-backend
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

export HATCH_JUPYTER_BUILDER_SKIP_NPM=1
export ARGON2_CFFI_USE_SYSTEM=1
pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
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

install -d "$PKG/etc"
mv "$PKG/usr/etc/jupyter" "$PKG/etc/jupyter"
rmdir "$PKG/usr/etc"

install -Dm644 /dev/stdin \
	"$PKG/etc/jupyter/jupyter_server_config.d/kdos-offline.json" <<'JSON'
{
  "LabApp": {
    "extension_manager": "readonly",
    "news_url": null,
    "check_for_updates_class": "jupyterlab.NeverCheckForUpdate"
  }
}
JSON

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	for i in jupyterlab notebook; do
		rsvg-convert -w $s -h $s \
			"$PKG/usr/share/icons/hicolor/scalable/apps/$i.svg" \
			-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/$i.png"
	done
done

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

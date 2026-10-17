# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_home=/usr/lib/python3-sphinx

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	alabaster babel imagesize requests charset-normalizer idna urllib3 \
	roman-numerals snowballstemmer sphinxcontrib-applehelp \
	sphinxcontrib-devhelp sphinxcontrib-htmlhelp sphinxcontrib-jsmath \
	sphinxcontrib-qthelp sphinxcontrib-serializinghtml \
	myst-parser markdown-it-py mdit-py-plugins mdurl sphinx-argparse .

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/sphinx-build" <<'KDOS_SH'
#!/bin/sh
home=/usr/lib/python3-sphinx
site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$home") || exit 1
PYTHONPATH=$site${PYTHONPATH:+:$PYTHONPATH}
export PYTHONPATH
"$home/bin/${0##*/}" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/sphinx-build"
for _cmd in sphinx-quickstart sphinx-apidoc sphinx-autogen; do
	[ -x "$PKG$_home/bin/$_cmd" ] || { echo "python3-sphinx: no $_cmd entry point" >&2; exit 1; }
	ln -s sphinx-build "$PKG/usr/bin/$_cmd"
done

PYTHONPATH=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG$_home") \
	python3 -m sphinx -b man -q -d "$SRC_ROOT/sphinx-doctrees" \
	-D extensions=sphinx.ext.autodoc,sphinx.ext.doctest,sphinx.ext.todo,sphinx.ext.autosummary,sphinx.ext.extlinks,sphinx.ext.viewcode,sphinx.ext.inheritance_diagram,sphinx.ext.coverage,sphinx.ext.graphviz \
	doc "$SRC_ROOT/sphinx-man"
install -Dm644 "$SRC_ROOT"/sphinx-man/*.1 -t "$PKG/usr/share/man/man1"

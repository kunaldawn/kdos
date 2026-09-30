# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLOSURE LIVES UNDER ITS OWN PREFIX, not in site-packages. requests,
# urllib3, idna and charset-normalizer are also vendored into site-packages by
# other ports, and two packages owning one path is a file conflict that stops
# the install. Under /usr/lib/python3-sphinx nothing else can own them. The
# libraries that ARE ports — docutils, jinja2, markupsafe, pygments, packaging,
# certifi, yaml — come from site-packages through `depends`.
#
# myst-parser is here because LLVM's and LLDB's documentation roots are
# Markdown: without it their man builders stop on a missing index document.
# sphinx-argparse is here because khard's pages are generated from its
# argument parser by that extension's directive.
#
# The consequence: `import sphinx` from a plain python3 fails. Only the
# commands in /usr/bin reach it, and every consumer here calls sphinx-build.
#
# --ignore-installed, because a copy already in the build root's site-packages
# would otherwise satisfy the requirement and leave the prefix without it.
# Build isolation is off: the backends are the flit-core, setuptools, hatchling
# and hatch-vcs ports, and an isolated build would fetch them.
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

# ONE LAUNCHER, FOUR NAMES. It resolves the prefix's site directory through
# sysconfig — the scheme pip installed with — so it names no Python version,
# prepends it to any PYTHONPATH the caller set (LLVM passes its own docs
# helpers that way) and runs the entry point of the name it was invoked by.
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

# The port's own pages come from its own builder, run from the staging tree.
# man_pages in doc/conf.py names them; nothing is fetched for them.
PYTHONPATH=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG$_home") \
	python3 -m sphinx -b man -q -d "$SRC_ROOT/sphinx-doctrees" doc "$SRC_ROOT/sphinx-man"
install -Dm644 "$SRC_ROOT"/sphinx-man/*.1 -t "$PKG/usr/share/man/man1"

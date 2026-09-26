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

# --no-deps, AND THE PART OF THE CLOSURE NO OTHER PROGRAM IMPORTS NAMED ONE BY
# ONE. The bundle is ipython's whole runtime set, and a resolving install would
# leave out whatever another port had already put in the build root and
# package the rest: which package owned psutil, pygments or wcwidth would
# follow build order. Those three are ports in `depends`; the bundle's copies
# are never installed. Build isolation stays on and pointed at the bundle, which
# carries every backend these sdists name, so nothing but $PKG is written.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr \
	asttokens executing ipython-pygments-lexers jedi matplotlib-inline parso \
	pexpect prompt-toolkit ptyprocess pure-eval stack-data traitlets .
install -Dm644 docs/man/ipython.1 -t "$PKG/usr/share/man/man1"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/ipython.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Python Shell
GenericName=Interactive Python
Comment=A Python prompt with completion and history
Exec=ipython
Icon=system-run
Terminal=true
Categories=Development;
Keywords=python;repl;shell;notebook;ipython;
EOF
chmod 644 "$PKG/usr/share/applications/ipython.desktop"

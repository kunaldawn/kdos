# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A SINGLE .py FILE AND NO DEPENDENCIES, which is why it is here rather than in
# a box: the ported gdb is already --with-python, so this is one file on disk
# and one line in a gdbinit. A crash in a boxed program is still debugged from
# the host.
#
# THE SOURCE IS NOT AN ARCHIVE, so kpkg leaves it in $PORT_SRC rather than
# unpacking it into $SRC — which is why this reads from there.
install -Dm644 "$PORT_SRC/$name-$version.py" $PKG/usr/share/gef/gef.py

# NOT SOURCED FROM /etc/skel's .gdbinit: gef takes over gdb's whole prompt, and
# somebody who wanted plain gdb would have no way back. The line to add is
# printed instead.
install -d $PKG/usr/share/doc/gef
cat > $PKG/usr/share/doc/gef/README.kdos << 'EOF'
gef is installed at /usr/share/gef/gef.py and is NOT enabled by default,
because it replaces gdb's entire prompt and a session that wanted plain gdb
would have no way back.

To use it once:      gdb -q -x /usr/share/gef/gef.py <program>
To use it always:    echo 'source /usr/share/gef/gef.py' >> ~/.gdbinit
EOF

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

install -Dm644 "$PORT_SRC/$name-$version.py" $PKG/usr/share/gef/gef.py

install -d $PKG/usr/share/doc/gef
cat > $PKG/usr/share/doc/gef/README.kdos << 'EOF'
gef is installed at /usr/share/gef/gef.py and is NOT enabled by default,
because it replaces gdb's entire prompt and a session that wanted plain gdb
would have no way back.

To use it once:      gdb -q -x /usr/share/gef/gef.py <program>
To use it always:    echo 'source /usr/share/gef/gef.py' >> ~/.gdbinit
EOF

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

install -dm755 $PKG/usr/share/lynis
cp -a db include plugins default.prf lynis $PKG/usr/share/lynis/
install -Dm644 lynis.8 $PKG/usr/share/man/man8/lynis.8

install -dm755 $PKG/usr/bin
cat > $PKG/usr/bin/lynis <<'SH'
#!/bin/sh
exec /usr/share/lynis/lynis --pentest "$@"
SH
chmod 755 $PKG/usr/bin/lynis

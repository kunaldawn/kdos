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

make full-ttf

install -dm755 $PKG/etc/fonts/conf.avail
install -dm755 $PKG/etc/fonts/conf.d
install -dm755 $PKG/usr/share/fonts/TTF
install -m644 build/*.ttf $PKG/usr/share/fonts/TTF/

for config in fontconfig/*.conf; do
	case $config in *-lgc-*) continue ;; esac
	install -m644 "$config" $PKG/etc/fonts/conf.avail/
	ln -sf ../conf.avail/${config##*/} $PKG/etc/fonts/conf.d/${config##*/}
done

install -Dm644 LICENSE $PKG/usr/share/licenses/$name/LICENSE

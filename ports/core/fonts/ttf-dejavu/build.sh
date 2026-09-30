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


# The faces are compiled from upstream's FontForge sources (src/*.sfd) by
# upstream's own Makefile: generate.pe has fontforge write each TrueType file
# and ttpostproc.pl rewrites its tables through Font::TTF. `full-ttf` is the
# 22 faces and nothing else; the default target also regenerates the coverage
# reports, which need Unicode and fontconfig orthography data this tree does
# not carry. SOURCE_DATE_EPOCH, set by the phase, is what fontforge stamps
# into each face's head table in place of the time of the build.
make full-ttf

install -dm755 $PKG/etc/fonts/conf.avail
install -dm755 $PKG/etc/fonts/conf.d
install -dm755 $PKG/usr/share/fonts/TTF
install -m644 build/*.ttf $PKG/usr/share/fonts/TTF/

# The LGC configurations belong to the LGC faces, which are not built.
for config in fontconfig/*.conf; do
	case $config in *-lgc-*) continue ;; esac
	install -m644 "$config" $PKG/etc/fonts/conf.avail/
	ln -sf ../conf.avail/${config##*/} $PKG/etc/fonts/conf.d/${config##*/}
done

install -Dm644 LICENSE $PKG/usr/share/licenses/$name/LICENSE

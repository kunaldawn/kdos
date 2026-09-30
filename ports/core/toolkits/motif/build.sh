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

# THE TARBALL SHIPS NO configure. It is a tag archive of the repository, so the
# autotools pass is run here rather than by upstream.
autoreconf -fi

# YACC is named rather than probed: configure takes byacc first and otherwise
# whatever `yacc` is on the path, and the uil grammar is built by bison here.
#
# --with-xinerama=no: RandR is the monitor source. configure falls back to
# Xinerama only when RandR is absent, and a build with neither would place
# every dialog on the first screen.
#
# GLw is off: it needs gl.pc, and no consumer of this port draws OpenGL
# through a Motif widget.
#
# --with-mwmrcdir moves mwm's rc file off /etc/X11, which must not exist on
# this image; the file is removed below with mwm itself.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-demos \
	--disable-tests \
	--disable-debug \
	--disable-message-catalog \
	--disable-glw \
	--enable-utf8 \
	--with-png \
	--with-jpeg \
	--with-xft \
	--with-xrandr \
	--with-xrender \
	--with-xcursor \
	--with-xinerama=no \
	--with-mwmrcdir=/usr/share/X11/mwm \
	YACC='bison -y'
make
make DESTDIR=$PKG install

# mwm AND xmbind ARE NOT SHIPPED. mwm is an X window manager, and X windows on
# this desktop are managed by kdos-comp's Xwayland window manager. xmbind loads
# key bindings into an X server's root window, which Xwayland does not keep
# between sessions. What the library needs, the virtual-key bindings, stays in
# /usr/share/X11/bindings.
rm -f "$PKG/usr/bin/mwm" "$PKG/usr/bin/xmbind" \
	"$PKG/usr/share/man/man1/mwm.1" "$PKG/usr/share/man/man1/xmbind.1" \
	"$PKG/usr/share/man/man4/mwmrc.4" \
	"$PKG/usr/share/X11/mwm/system.mwmrc"
rmdir "$PKG/usr/share/X11/mwm"

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

# pinentry-qt IS THE DEFAULT, and /usr/bin/pinentry is a link to it. It draws
# a Qt 6 dialog when gpg-agent's request carries WAYLAND_DISPLAY or DISPLAY,
# which gpg passes along, so a program started from the menu gets a prompt.
# With neither it falls back to the curses prompt on GPG_TTY; that fallback is
# named with --enable-fallback-curses rather than left to detection.
# KF6GuiAddons gives the dialog its Caps Lock warning on Wayland and
# KF6WindowSystem parents it to the window that asked. configure only warns
# when either is missing, so the config.h check after it fails the build.
#
# Every other graphical front end is off. gtk2 and the Qt 5 one would each put
# a toolkit on the machine for a second copy of the same dialog; gnome3 is gcr's
# system prompt, which only a GNOME session answers; EFL and FLTK are not
# ports. Most default to `maybe`, and would otherwise appear the moment their
# toolkit happened to be installed.
./configure --prefix=/usr \
	--enable-pinentry-curses \
	--enable-pinentry-tty \
	--enable-pinentry-qt \
	--enable-fallback-curses \
	--disable-pinentry-emacs \
	--disable-pinentry-efl \
	--disable-pinentry-gtk2 \
	--disable-pinentry-gnome3 \
	--disable-pinentry-qt5 \
	--disable-pinentry-qt4 \
	--disable-pinentry-tqt \
	--disable-pinentry-fltk \
	--disable-libsecret
for have in PINENTRY_KGUIADDONS PINENTRY_KWINDOWSYSTEM; do
	grep -q "^#define $have 1" config.h || {
		echo "pinentry: $have not configured" >&2
		exit 1
	}
done
make
make DESTDIR=$PKG install

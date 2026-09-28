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

# THE SDL 2 FRONT END, one of the four configure accepts, drawn by sdl2-compat
# on SDL 3's Wayland driver; its menus are drawn inside the emulated screen.
# The GTK front end would put an X11 child window under its OpenGL view.
#
# configure refuses any --enable/--with it does not list, so the usual
# --disable-static is not passed. It also requires dos2unix and xa at
# configure time. dos2unix runs only in the dist targets, never in a build, so
# the preset variable answers the check. xa is real: it assembles the PSID
# player that VSID loads into the emulated machine.
#
# Every codec is named, because configure answers a missing one by leaving
# the feature out: FLAC, Ogg Vorbis and mpg123 for the sampler and tape
# formats, LAME for MP3 recording, PNG and GIF screenshots. Sound through
# SDL, ALSA or PulseAudio (pipewire-pulse), MIDI through ALSA sequencer.
# libcurl serves only WIC64, a cartridge that reaches the internet, and is
# off; PortAudio is a second route to the capture device ALSA already reaches.
# Real IEC hardware (OpenCBM) and the Ethernet cartridges need root or a
# device this image does not carry, and are off.
DOS2UNIX=true ./configure --prefix=/usr --libdir=/usr/lib \
	--enable-sdl2ui \
	--with-sdlsound \
	--with-alsa \
	--with-pulse \
	--without-oss \
	--with-flac \
	--with-vorbis \
	--with-mpg123 \
	--with-lame \
	--with-png \
	--with-gif \
	--with-resid \
	--with-fastsid \
	--without-libcurl \
	--without-portaudio \
	--enable-midi \
	--disable-realdevice \
	--disable-ethernet \
	--enable-html-docs \
	--disable-pdf-docs
make
make DESTDIR=$PKG install

# The SDL front end installs no icons or menu entries. The ROM sets and the
# PNG icons go in with the emulators; x64sc and xcbm5x0 have no icon of their
# own and share their family's.
for _size in 16 24 32 48 64 256; do
	_dir="$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps"
	install -d "$_dir"
	for _emu in vsid x64 x64dtv x128 xcbm2 xpet xplus4 xscpu64 xvic; do
		install -m644 "data/common/vice-${_emu}_${_size}.png" "$_dir/vice-${_emu}.png"
	done
done

# One entry per machine. Each emulator is its own program, and SDL's Wayland
# app_id is the executable's name.
_entry() {
	cat > "$PKG/usr/share/applications/vice-$1.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=$3
GenericName=$4
Comment=$5
TryExec=$1
Exec=$1 %f
Icon=vice-$2
Terminal=false
StartupWMClass=$1
Categories=Game;Emulator;
Keywords=vice;commodore;cbm;emulator;retro;$6
DESKTOP
	chmod 644 "$PKG/usr/share/applications/vice-$1.desktop"
}
install -d "$PKG/usr/share/applications"
_entry x64sc   x64     'VICE C64'           'Commodore 64 Emulator'    'Emulate the Commodore 64'                          'c64;commodore 64;'
_entry xscpu64 xscpu64 'VICE C64 SuperCPU'  'Commodore 64 Emulator'    'Emulate the Commodore 64 with a SuperCPU'          'c64;supercpu;'
_entry x64dtv  x64dtv  'VICE C64 DTV'       'Commodore 64 Emulator'    'Emulate the C64 Direct-to-TV joystick'             'c64;dtv;'
_entry x128    x128    'VICE C128'          'Commodore 128 Emulator'   'Emulate the Commodore 128'                         'c128;commodore 128;'
_entry xvic    xvic    'VICE VIC-20'        'VIC-20 Emulator'          'Emulate the Commodore VIC-20'                      'vic20;vic-20;'
_entry xplus4  xplus4  'VICE Plus/4'        'Plus/4 Emulator'          'Emulate the Commodore Plus/4, C16 and C116'        'plus4;c16;ted;'
_entry xpet    xpet    'VICE PET'           'PET Emulator'             'Emulate the Commodore PET and SuperPET'            'pet;superpet;'
_entry xcbm2   xcbm2   'VICE CBM-II'        'CBM-II Emulator'          'Emulate the CBM-II 600 and 700 series'             'cbm2;b128;'
_entry xcbm5x0 xcbm2   'VICE CBM 5x0'       'CBM-II Emulator'          'Emulate the CBM-II 500 series (P500)'              'cbm2;p500;'
_entry vsid    vsid    'VICE SID Player'    'SID Music Player'         'Play Commodore 64 SID music files'                 'sid;music;chiptune;'

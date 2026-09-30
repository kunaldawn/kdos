# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# YTK's hard-coded config claims glibc's nftw() flags on every Linux; musl's
# nftw() has none of them, so the patch keys them on the flags themselves.
patch -p1 -i "$PORT_SRC/musl.patch"

# Ardour's toolkit, YTK, is an in-tree GTK 2 fork that draws through X11
# only, so the program is an Xwayland window. PipeWire answers the ALSA and
# PulseAudio backends; there is no JACK. The start-up news fetch from
# ardour.org is compiled out, and the interface is English only.
python3 ./waf configure \
	--prefix=/usr \
	--configdir=/etc \
	--libdir=/usr/lib \
	--optimize \
	--freedesktop \
	--no-phone-home \
	--no-nls \
	--with-backends=alsa,pulseaudio,dummy \
	--libjack=weak \
	--noconfirm
python3 ./waf build $MAKEFLAGS
python3 ./waf install --destdir="$PKG"

install -Dm644 ardour.1 "$PKG/usr/share/man/man1/ardour9.1"

cat > "$PKG/usr/share/applications/ardour9.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Ardour
GenericName=Digital Audio Workstation
Comment=Record, edit and mix multitrack audio and MIDI
Exec=ardour9
Icon=ardour9
Terminal=false
StartupWMClass=Ardour
Categories=AudioVideo;Audio;AudioVideoEditing;Recorder;Mixer;
MimeType=application/x-ardour;
Keywords=daw;audio;recording;mixing;multitrack;midi;lv2;
DESKTOP
chmod 644 "$PKG/usr/share/applications/ardour9.desktop"

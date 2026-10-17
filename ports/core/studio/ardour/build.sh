# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/musl.patch"

python3 ./waf configure \
	--prefix=/usr \
	--configdir=/etc \
	--libdir=/usr/lib \
	--optimize \
	--freedesktop \
	--no-phone-home \
	--no-nls \
	--with-backends=jack,alsa,pulseaudio,dummy \
	--libjack=link \
	--cxx17 \
	--no-execstack \
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

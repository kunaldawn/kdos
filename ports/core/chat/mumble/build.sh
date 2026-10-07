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

# musl declares ioctl()'s request as int, and the evdev request macros expand
# to unsigned values that overflow the conversion under -Werror=narrowing.
patch -p1 -i "$PORT_SRC/EVIO-int.patch"
# The shipped mumble-server.ini keeps its database where the service account
# owns a directory, instead of searching the working directory for one.
patch -p1 -i "$PORT_SRC/server-ini.patch"
# OpenSSL 4 returns a certificate's subject name read-only, and the
# self-signed certificate generator added its common name to that one; the
# patch builds a name of its own and sets it as both subject and issuer.
patch -p1 -i "$PORT_SRC/openssl4-x509-name.patch"

# update and crash-report are off: both reach mumble.info. translations and
# bundle-qt-translations are off: bundled data is English only. jackaudio,
# portaudio and oss are off; PipeWire and PulseAudio are dlopen()ed at run time
# against the headers in 3rdparty/, and ALSA is linked. speechd reads messages
# aloud through speech-dispatcher; qtspeech needs a Qt 5 QtSpeech this tree
# does not build. xinput2 and the X11/Xext link serve global shortcuts, which
# work only for X11 windows. zeroconf browses and announces servers on the LAN
# through avahi's libdns_sd compatibility library. The overlay is an
# LD_PRELOAD library for GLX games and is not built. ice is off: ZeroC Ice is
# not a port. GSL (header-only) and ReNameNoise are compiled from 3rdparty/
# because neither is a port; speex and nlohmann_json come from their ports.
# use-timestamps off keeps __DATE__ out of the binaries. C++17, not upstream's
# 14: protobuf's headers (through abseil) and POCO's both require it.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DCMAKE_CXX_STANDARD=17 \
	-DBUILD_NUMBER="${version##*.}" \
	-Dclient=ON \
	-Dserver=ON \
	-Dtests=OFF \
	-Dbenchmarks=OFF \
	-Dplugins=ON \
	-Dwarnings-as-errors=OFF \
	-Duse-timestamps=OFF \
	-Dupdate=OFF \
	-Dcrash-report=OFF \
	-Dtranslations=OFF \
	-Dbundle-qt-translations=OFF \
	-Dbundled-speex=OFF \
	-Dbundled-json=OFF \
	-Dbundled-gsl=ON \
	-Drenamenoise=ON \
	-Dbundled-renamenoise=ON \
	-Dalsa=ON \
	-Dpipewire=ON \
	-Dpulseaudio=ON \
	-Djackaudio=OFF \
	-Dportaudio=OFF \
	-Doss=OFF \
	-Dspeechd=ON \
	-Dqtspeech=OFF \
	-Dxinput2=ON \
	-Dg15=OFF \
	-Dzeroconf=ON \
	-Dqssldiffiehellmanparameters=ON \
	-Doverlay=OFF \
	-Doverlay-xcompile=OFF \
	-Dice=OFF \
	-Dmanual-plugin=ON
cmake --build build
DESTDIR=$PKG cmake --install build

# The systemd unit, sysusers and tmpfiles entries have nothing to read them,
# and the per-user wrapper script drives a server by hand in ~/mumble-server;
# the service below replaces all four.
rm -rf "$PKG/etc/systemd" "$PKG/etc/sysusers.d" "$PKG/etc/tmpfiles.d"
rm -f "$PKG/usr/bin/mumble-server-user-wrapper" \
	"$PKG/usr/share/man/man1/mumble-server-user-wrapper.1"
install -d -m 750 "$PKG/var/lib/mumble-server"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: main.cpp sets the desktop
# file name, so the Wayland app_id is info.mumble.Mumble, not the X11 class
# `mumble` upstream names.
cat > "$PKG/usr/share/applications/info.mumble.Mumble.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mumble
GenericName=Voice Chat
Comment=Low-latency group voice chat
Exec=mumble %u
Icon=mumble
Terminal=false
StartupWMClass=info.mumble.Mumble
MimeType=x-scheme-handler/mumble;
Categories=Network;Chat;AudioVideo;Qt;
Keywords=voip;voice;chat;talk;mumble;murmur;
DESKTOP
chmod 644 "$PKG/usr/share/applications/info.mumble.Mumble.desktop"

# THE SERVER RUNS UNDER ksvc, AS ITS ACCOUNT. mumble-server drops privileges
# only when told a uname in its ini, so setpriv makes it the postinstall's
# account before the exec; -fg keeps it in the foreground and logging to the
# supervisor.
#
# IT SKIPS UNTIL A DATABASE EXISTS. Setting the SuperUser password creates it:
#   setpriv --reuid=mumble-server --regid=mumble-server --init-groups \
#       mumble-server -ini /etc/mumble/mumble-server.ini -supw <password>
# Without that step every machine carrying the port would hold 64738 open for a
# server nobody administers.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/75_mumble-server.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="mumble-server"
DAEMON="/usr/bin/mumble-server"
INI="/etc/mumble/mumble-server.ini"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^mumble-server:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no mumble-server account (the port's postinstall makes it)"
            exit 0
        fi
        if [ ! -s /var/lib/mumble-server/mumble-server.sqlite ]; then
            echo "[SKIP] $NAME: no database yet ('mumble-server -supw' as the account makes one)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=mumble-server --regid=mumble-server \
            --init-groups "$DAEMON" -fg -ini "$INI"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/75_mumble-server.sh"

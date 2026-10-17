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

patch -p1 -i "$PORT_SRC/EVIO-int.patch"
patch -p1 -i "$PORT_SRC/server-ini.patch"

_ossl="$SRC_ROOT/openssl-$_openssl"
(
	cd "$_ossl"
	./config \
		--prefix=/usr \
		--libdir=lib \
		--openssldir=/etc/ssl \
		enable-ec_nistp_64_gcc_128 \
		enable-camellia \
		enable-seed \
		enable-rfc3779 \
		enable-ktls \
		enable-argon2 \
		no-mdc2 \
		no-ec2m \
		no-sm2 \
		no-sm4 \
		no-module \
		no-tests \
		shared \
		threads \
		zlib
	make build_generated
)
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
	-Dmanual-plugin=ON \
	-DCMAKE_IGNORE_PATH="/usr/lib/cmake/OpenSSL;/lib/cmake/OpenSSL" \
	-DOPENSSL_INCLUDE_DIR="$_ossl/include" \
	-DOPENSSL_SSL_LIBRARY=/usr/lib/libssl.so.3 \
	-DOPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.so.3
cmake --build build
DESTDIR=$PKG cmake --install build
for _b in mumble mumble-server; do
	_need=$(readelf -d "$PKG/usr/bin/$_b")
	case $_need in *libssl.so.3*) ;; *) exit 1 ;; esac
	case $_need in *libssl.so.4*|*libcrypto.so.4*) exit 1 ;; esac
done

rm -rf "$PKG/etc/systemd" "$PKG/etc/sysusers.d" "$PKG/etc/tmpfiles.d"
rm -f "$PKG/usr/bin/mumble-server-user-wrapper" \
	"$PKG/usr/share/man/man1/mumble-server-user-wrapper.1"
install -d -m 750 "$PKG/var/lib/mumble-server"

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

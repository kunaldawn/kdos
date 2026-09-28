# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The update check asks the npm registry for a newer release on every page
# load unless it is switched off; it starts off.
patch -p1 -i "$PORT_SRC/no-update-check.patch"

# The bundle is node_modules and its package-lock.json as npm resolves
# package.json with --legacy-peer-deps and --ignore-scripts; the lock file is
# what lets npm prune work offline. Upstream resolves with Yarn, whose lock
# file npm cannot read, and one of its pins conflicts under npm's
# strict peer check. Prebuilt binaries in it are removed; the serial port
# binding, the one native module CNCjs runs, is compiled here.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
rm -rf node_modules/@serialport/bindings-cpp/prebuilds node_modules/@parcel/watcher-* \
	node_modules/@unrs node_modules/app-builder-bin node_modules/7zip-bin \
	node_modules/term-size/vendor
export PATH="$SRC/node_modules/.bin:$PATH"
_nodedir=$(dirname "$(dirname "$(command -v node)")")

# scripts/build-prod.sh, step by step, without Yarn.
node scripts/package-sync.js
mkdir -p dist/cncjs
cp -af src/package.json dist/cncjs/
( cd src && NODE_ENV=production babel "*.js" --config-file ../babel.config.js --out-dir ../dist/cncjs )
NODE_ENV=production babel -d dist/cncjs/server src/server
NODE_ENV=production webpack-cli --config webpack.config.production.js
mkdir -p dist/cncjs/app dist/cncjs/server
cp -af src/app/favicon.ico src/app/i18n src/app/images src/app/assets dist/cncjs/app/
cp -af src/server/i18n src/server/views dist/cncjs/server/
find dist -name '*.map' -delete
# English only: the other interface catalogues go.
find dist/cncjs/app/i18n dist/cncjs/server/i18n -mindepth 1 -maxdepth 1 -type d ! -name en \
	-exec rm -rf {} +

( cd node_modules/@serialport/bindings-cpp && node-gyp rebuild --nodedir="$_nodedir" )

# Only what the server imports at run time is installed.
npm prune --omit=dev --offline --ignore-scripts --legacy-peer-deps --no-audit --no-fund

_dir=/usr/lib/cncjs
install -d "$PKG$_dir/bin" "$PKG/usr/bin"
cp -a dist node_modules package.json "$PKG$_dir/"
install -m755 bin/cncjs "$PKG$_dir/bin/cncjs"
ln -s ../lib/cncjs/bin/cncjs "$PKG/usr/bin/cncjs"

install -Dm644 src/app/images/logo-square-256x256.png "$PKG/usr/share/icons/hicolor/256x256/apps/cncjs.png"

# The controller is a web page: the entry starts the server in a terminal,
# and the interface is http://localhost:8000 in the browser. The server
# listens on every interface unless told otherwise, and it drives the
# machine without a login, so the entry binds it to loopback.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/cncjs.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=CNCjs Server
GenericName=CNC Controller
Comment=Serve the CNCjs machine controller at http://localhost:8000
Exec=cncjs --host 127.0.0.1
Icon=cncjs
Terminal=true
Categories=Utility;Engineering;
Keywords=cnc;grbl;marlin;smoothie;tinyg;gcode;g-code;mill;laser;
DESKTOP
chmod 644 "$PKG/usr/share/applications/cncjs.desktop"

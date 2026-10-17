# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/no-update-check.patch"

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
rm -rf node_modules/@serialport/bindings-cpp/prebuilds node_modules/@parcel/watcher-* \
	node_modules/@unrs node_modules/app-builder-bin node_modules/7zip-bin \
	node_modules/term-size/vendor
export PATH="$SRC/node_modules/.bin:$PATH"
_nodedir=$(dirname "$(dirname "$(command -v node)")")

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
find dist/cncjs/app/i18n dist/cncjs/server/i18n -mindepth 1 -maxdepth 1 -type d ! -name en \
	-exec rm -rf {} +

( cd node_modules/@serialport/bindings-cpp && node-gyp rebuild --nodedir="$_nodedir" )

npm prune --omit=dev --offline --ignore-scripts --legacy-peer-deps --no-audit --no-fund

_dir=/usr/lib/cncjs
install -d "$PKG$_dir/bin" "$PKG/usr/bin"
cp -a dist node_modules package.json "$PKG$_dir/"
find "$PKG$_dir/node_modules" \( -name '*.orig' -o -name '*.rej' \) -delete
install -m755 bin/cncjs "$PKG$_dir/bin/cncjs"
ln -s ../lib/cncjs/bin/cncjs "$PKG/usr/bin/cncjs"

install -Dm644 src/app/images/logo-square-256x256.png "$PKG/usr/share/icons/hicolor/256x256/apps/cncjs.png"

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

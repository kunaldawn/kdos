# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cc $CFLAGS $LDFLAGS -Wno-deprecated-non-prototype -Wno-format-truncation \
   -Wno-unused-result predict.c -lncurses -pthread -lasound -lcurl -lm \
   -o predict \
   -DBUILD_DIR="\"/usr/share/predict/\"" \
   -DPREDICT_VERSION="\"$version\""

install -Dm755 predict $PKG/usr/bin/predict
install -Dm644 docs/man/predict.1 $PKG/usr/share/man/man1/predict.1
install -d $PKG/usr/share/predict
cp -r default vocalizer $PKG/usr/share/predict/

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/predict.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Satellites (predict)
GenericName=Satellite Tracker
Comment=Passes, doppler and footprints from a TLE
Exec=predict
Icon=network-wireless
Terminal=true
Categories=Science;Astronomy;HamRadio;
Keywords=satellite;tle;orbit;pass;doppler;ham;radio;predict;
EOF
chmod 644 "$PKG/usr/share/applications/predict.desktop"

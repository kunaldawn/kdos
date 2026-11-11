# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/no-wx-on-console.patch"

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

rm -f $PKG/usr/bin/pronterface.py $PKG/usr/bin/plater.py
rm -f $PKG/usr/bin/__pycache__/pronterface.*.pyc $PKG/usr/bin/__pycache__/plater.*.pyc

rm -f $PKG/usr/share/applications/pronterface.desktop \
      $PKG/usr/share/applications/plater.desktop \
      $PKG/usr/share/applications/pronsole.desktop \
      $PKG/usr/share/metainfo/pronterface.appdata.xml \
      $PKG/usr/share/metainfo/plater.appdata.xml

ln -s pronsole.py $PKG/usr/bin/pronsole
ln -s printcore.py $PKG/usr/bin/printcore

cat > "$PKG/usr/share/applications/pronsole.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Pronsole
GenericName=3D Printing Console
Comment=Send G-code to a RepRap machine and follow the print from a terminal
Exec=pronsole
Icon=pronsole
Terminal=true
Categories=Utility;Graphics;3DGraphics;ConsoleOnly;
Keywords=3d;printing;reprap;gcode;g-code;printrun;pronsole;
DESKTOP
chmod 644 "$PKG/usr/share/applications/pronsole.desktop"

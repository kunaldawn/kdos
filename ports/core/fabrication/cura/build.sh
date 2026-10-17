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


mkdir -p vendor
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr trimesh zeroconf ifaddr

_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib", vars={"base": "/usr"}))')
install -d "$PKG$_site" "$PKG/usr/lib/cura/plugins" "$PKG/usr/share/cura"
cp -r cura "$PKG$_site/"
cp -r "$SRC_ROOT/libCharon-$_charon/Charon" "$PKG$_site/"

cat > "$PKG$_site/cura/CuraVersion.py" <<PY
CuraAppName = "cura"
CuraAppDisplayName = "UltiMaker Cura"
CuraVersion = "$version"
CuraVersionFull = "$version"
CuraBuildType = ""
CuraDebugMode = False
CuraCloudAPIRoot = "https://api.ultimaker.com"
CuraCloudAPIVersion = "1"
CuraCloudAccountAPIRoot = "https://account.ultimaker.com"
CuraMarketplaceRoot = "https://marketplace.ultimaker.com"
CuraDigitalFactoryURL = "https://digitalfactory.ultimaker.com"
CuraLatestURL = "https://software.ultimaker.com/latest.json"

ConanInstalls = {}

PythonInstalls = {}

DependenciesDescriptions = {}
PY

for p in plugins/*/; do
	p=${p%/}
	case ${p##*/} in
	SentryLogger|SliceInfoPlugin|FirmwareUpdateChecker|3DConnexion|MakerbotWriter) continue ;;
	esac
	cp -r "$p" "$PKG/usr/lib/cura/plugins/"
done

cp -r resources "$PKG/usr/share/cura/"
rm -rf "$PKG/usr/share/cura/resources/i18n"
install -d "$PKG/usr/share/cura/resources/materials"
cp "$SRC_ROOT/fdm_materials-$version"/*.xml.fdm_material \
	"$PKG/usr/share/cura/resources/materials/"

find "$PKG$_site/cura" "$PKG$_site/Charon" "$PKG/usr/lib/cura" -depth -type d \
	\( -name tests -o -name __pycache__ \) -exec rm -rf {} +
find "$PKG$_site/cura" "$PKG$_site/Charon" "$PKG/usr/lib/cura" "$PKG/usr/share/cura" \
	-type f -exec chmod 644 {} +
python3 -m compileall -q -s "$PKG" -p / "$PKG$_site/cura" "$PKG$_site/Charon" "$PKG/usr/lib/cura"

install -Dm644 cura_app.py "$PKG/usr/lib/cura/cura_app.py"
cat > "$PKG/usr/bin/cura" <<'PY'
#!/usr/bin/python3
from PyQt6.QtGui import QGuiApplication
QGuiApplication.setDesktopFileName("cura")
_path = "/usr/lib/cura/cura_app.py"
with open(_path) as _f:
    _code = compile(_f.read(), _path, "exec")
exec(_code, {"__name__": "__main__", "__file__": _path})
PY
chmod 755 "$PKG/usr/bin/cura"

for s in 64 128 256; do
	install -Dm644 "packaging/icons/cura-icon_${s}x${s}.png" \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/cura-icon.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/cura.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=UltiMaker Cura
GenericName=3D Printing Slicer
Comment=Prepare 3D models for printing on FDM printers
Exec=cura %F
Icon=cura-icon
Terminal=false
Categories=Qt;Graphics;3DGraphics;Engineering;
Keywords=3d;printing;slicer;gcode;stl;3mf;cura;ultimaker;
StartupWMClass=cura
DESKTOP
chmod 644 "$PKG/usr/share/applications/cura.desktop"

test -f "$PKG/usr/share/cura/resources/definitions/fdmprinter.def.json"
test -n "$(ls "$PKG/usr/share/cura/resources/materials")"
test -f "$PKG/usr/lib/cura/plugins/CuraEngineBackend/plugin.json"

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

P="$PKG/usr/share/plasma/plasmoids/org.kdos.fixture.generated"
install -Dm644 "$PORT_SRC/package/metadata.json" "$P/metadata.json"
install -Dm644 "$PORT_SRC/package/contents/ui/main.qml" "$P/contents/ui/main.qml"
kdos-fixture-gen "$P/contents/ui"

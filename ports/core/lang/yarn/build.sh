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

install -Dm755 bin/yarn.js "$PKG/usr/lib/node_modules/yarn/bin/yarn.js"
install -Dm644 package.json "$PKG/usr/lib/node_modules/yarn/package.json"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/yarn" <<'KDOS_SH'
#!/bin/sh
export YARN_ENABLE_TELEMETRY="${YARN_ENABLE_TELEMETRY:-0}"
exec node /usr/lib/node_modules/yarn/bin/yarn.js "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/yarn"
ln -s yarn "$PKG/usr/bin/yarnpkg"

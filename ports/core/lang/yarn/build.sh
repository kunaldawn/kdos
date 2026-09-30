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

# cli-dist is Yarn's release: one bundled script run by node. A project pins
# its Yarn in package.json's packageManager and that version is what corepack
# would fetch; node no longer carries corepack, and a fetch has no network, so
# this port is the Yarn every build finds on PATH. Yarn does not compare
# packageManager with its own version; only a project's yarnPath setting hands
# the run to another copy, and that copy must then be in the source.
install -Dm755 bin/yarn.js "$PKG/usr/lib/node_modules/yarn/bin/yarn.js"
install -Dm644 package.json "$PKG/usr/lib/node_modules/yarn/package.json"

# Telemetry is on by default and reports to Yarn's servers on every run. The
# environment is the only setting that reaches every user and every project
# without a file in each home, so the command sets it before node starts,
# leaving a value the caller exported in place.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/yarn" <<'KDOS_SH'
#!/bin/sh
export YARN_ENABLE_TELEMETRY="${YARN_ENABLE_TELEMETRY:-0}"
exec node /usr/lib/node_modules/yarn/bin/yarn.js "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/yarn"
ln -s yarn "$PKG/usr/bin/yarnpkg"

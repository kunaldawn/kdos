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

# BUILDTAGS is set whole, which replaces upstream's probes: no systemd tag
# (KDOS has no systemd), no apparmor, no libsubid, no btrfs driver, and
# libsqlite3 so the database links the sqlite port instead of the copy bundled
# with the Go binding.
export BUILDTAGS="seccomp libsqlite3 exclude_graphdriver_btrfs"

# `binaries` builds podman, podman-remote, rootlessport, quadlet, etc.
make BUILDTAGS="$BUILDTAGS" \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	GOMD2MAN=/usr/bin/go-md2man \
	binaries docs

make DESTDIR=$PKG \
	PREFIX=/usr \
	ETCDIR=/etc \
	BINDIR=/usr/bin \
	LIBEXECPODMAN=/usr/lib/podman \
	install.bin install.remote install.man install.completions install.docker

# install.bin and install.docker always install quadlet's generator links under
# lib/systemd and tmpfiles.d entries: the directory variables only move them,
# and nothing here reads any of them. The csh half of the DOCKER_HOST profile
# goes too; no csh is installed.
rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/tmpfiles.d" "$PKG/usr/share/user-tmpfiles.d"
rm -f "$PKG/etc/profile.d/podman-docker.csh"

# /etc/containers is containers-common's: buildah and skopeo read it too.

# THE API SOCKET STARTS WHEN A CLIENT ASKS FOR IT AND STOPS WHEN NONE IS LEFT.
# lazydocker speaks the Docker API and podman-tui the Podman one, and both
# reach this user's engine only through `podman system service`. There is no
# socket activation without systemd, so each menu entry runs its tool behind
# this script: it starts the service if nothing answers on the socket, points
# DOCKER_HOST at it, and gives podman-tui a default connection to it through
# CONTAINERS_CONF_OVERRIDE, which a default the user set with `podman system
# connection` still outranks. The service runs with an idle timeout and exits
# by itself once every client has been gone that long, so a closed window
# leaves no daemon behind.
#
# The lock descriptor is closed for the service: it inherits every open one,
# and a service holding the lock would block every later launch until it
# exited.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/kdos-podman-api" <<'KDOS_SH'
#!/bin/sh
# kdos-podman-api COMMAND [ARG...] — run COMMAND against this user's Podman
# API socket, starting `podman system service` first if nothing answers on it.
[ $# -gt 0 ] || {
	echo "usage: kdos-podman-api COMMAND [ARG...]" >&2
	exit 2
}
if [ "$(id -u)" = 0 ]; then
	_dir=/run/podman
else
	_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/podman"
fi
_sock="$_dir/podman.sock"
_idle=300

_answers() {
	podman --url "unix://$_sock" version >/dev/null 2>&1
}

if ! _answers; then
	mkdir -p "$_dir" || exit 1
	(
		flock 9
		if ! _answers; then
			rm -f "$_sock"
			env -u CONTAINER_HOST -u CONTAINER_CONNECTION \
				setsid podman system service --time="$_idle" \
				"unix://$_sock" </dev/null >/dev/null 2>&1 9>&- &
			_i=0
			while [ "$_i" -lt 100 ] && ! _answers; do
				sleep 0.1
				_i=$((_i + 1))
			done
		fi
	) 9>"$_dir/.kdos-api.lock"
	_answers || {
		echo "kdos-podman-api: nothing answers on $_sock" >&2
		exit 1
	}
fi

DOCKER_HOST="unix://$_sock"
export DOCKER_HOST
if [ -z "${CONTAINERS_CONF_OVERRIDE:-}" ]; then
	printf '[engine]\nactive_service = "kdos-local"\n\n[engine.service_destinations.kdos-local]\nuri = "unix://%s"\n' \
		"$_sock" > "$_dir/kdos-api.conf.$$"
	mv -f "$_dir/kdos-api.conf.$$" "$_dir/kdos-api.conf"
	CONTAINERS_CONF_OVERRIDE="$_dir/kdos-api.conf"
	export CONTAINERS_CONF_OVERRIDE
fi
exec "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/kdos-podman-api"

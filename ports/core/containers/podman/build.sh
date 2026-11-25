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

patch -Np1 -i "$PORT_SRC/0001-exclude-systemd-commands-build-tag.patch"

export BUILDTAGS="seccomp libsqlite3 exclude_graphdriver_btrfs exclude_systemd_commands"
export REMOTETAGS="remote containers_image_openpgp exclude_graphdriver_btrfs exclude_systemd_commands"

export GOFLAGS="$GOFLAGS \"-ldflags=-s -w\""
make BUILDTAGS="$BUILDTAGS" \
	REMOTETAGS="$REMOTETAGS" \
	EXTRA_LDFLAGS="-s -w" \
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

rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/tmpfiles.d" "$PKG/usr/share/user-tmpfiles.d" "$PKG/usr/lib/podman/quadlet"
rm -f "$PKG/etc/profile.d/podman-docker.csh" "$PKG"/usr/share/man/man5/podman-*.unit.5 "$PKG"/usr/share/man/man5/quadlet.5 "$PKG"/usr/share/man/man1/podman-generate-systemd.1 "$PKG"/usr/share/man/man1/podman-quadlet*.1 "$PKG"/usr/share/man/man7/podman-quadlet*.7 "$PKG/usr/bin/podmansh" "$PKG/usr/share/man/man1/podmansh.1" "$PKG/usr/share/man/man1/podman-auto-update.1"

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/kdos-podman-api" <<'KDOS_SH'
#!/bin/sh
# kdos-podman-api COMMAND [ARG...] — run COMMAND against this user's Podman
# API socket, starting `podman system service` first if nothing answers on it.
case "$1" in
-h|--help)
	echo "usage: kdos-podman-api COMMAND [ARG...]"
	exit 0
	;;
''|-*)
	echo "usage: kdos-podman-api COMMAND [ARG...]" >&2
	exit 2
	;;
esac
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

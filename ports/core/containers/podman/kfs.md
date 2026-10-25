## About

Podman is a container engine that needs no daemon and runs as an ordinary user. In KDOS it is what
a [box]{.idx} is: every catalogue application runs in a rootless Podman container of its own,
which [kdos-appbox](#port-kdos-appbox) creates (through `distrobox create` for a catalogue
image, `podman create --rootfs` for a pack), starts with `podman start` and enters with
`podman exec`, and the catalogue's images are built with `podman build` on the machine that
installs them. [distrobox](#port-distrobox), [podman-tui](#port-podman-tui) and
[lazydocker](#port-lazydocker) sit on top of it. The launch path is in
[kdos-appbox](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-appbox.md#the-launch-path).

The port builds upstream's Go makefile with a set of build tags of its own, installs the targets one by
one instead of upstream's `install`, which would add systemd units, and removes what systemd alone
reads. Its configuration in `/etc/containers` belongs to
[containers-common](#port-containers-common), not to this package, because buildah and skopeo
read it too; the settings are in
[Containers](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/configuration.md#containers).

## Flags

- `BUILDTAGS` — Passed on the `make` command line, so it replaces upstream's whole list. That
  list is `grpcnotrace`, `seccomp` on Linux, and whatever the `hack/*_tag.sh` scripts print after
  probing the build machine: `apparmor`, `systemd`, `libsubid`, `libsqlite3` and the btrfs
  exclusion. Only the three tags below are set, whatever the build root holds; in particular there
  is no `systemd` tag, as KDOS has no systemd, and no `apparmor` or `libsubid`.
- `seccomp` — Compiles in seccomp filtering through [libseccomp](#port-libseccomp), so each
  container runs under the syscall filter its profile names.
- `libsqlite3` — Links the database Podman keeps its state in against the
  [sqlite](#port-sqlite) port, instead of compiling the copy of SQLite bundled with the Go
  binding.
- `exclude_graphdriver_btrfs` — Leaves out the btrfs storage driver. The store uses the overlay
  driver through [fuse-overlayfs](#port-fuse-overlayfs) (see `storage.conf`), and the btrfs driver
  would compile against [btrfs-progs](#port-btrfs-progs)' headers whenever they happened to be
  installed.
- `GOFLAGS="$GOFLAGS \"-ldflags=-s -w\""` — Extends the tree's exported `GOFLAGS` with `-s -w`,
  which drop the symbol table and DWARF, for any binary the makefile builds without an
  `-ldflags` of its own. See [go](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go).
- `EXTRA_LDFLAGS="-s -w"` — The same two flags for the binaries the makefile does link with its
  own `-ldflags`, which a `GOFLAGS` `-ldflags` cannot reach; upstream appends `EXTRA_LDFLAGS` to
  that string.
- `LIBEXECPODMAN=/usr/lib/podman` — Installs the helpers, `rootlessport` and `quadlet`, in
  `/usr/lib/podman`, which is one of the directories Podman searches for its helper programs and
  the one [catatonit](#port-catatonit) installs into. With `PREFIX=/usr` upstream's default would
  be `/usr/libexec/podman`.
- `GOMD2MAN=/usr/bin/go-md2man` — Converts the Markdown manual pages with the
  [go-md2man](#port-go-md2man) port. Upstream's default is a copy it builds itself from
  `test/tools`, a separate Go module.
- `install.remote` — Installs `podman-remote`, the client that drives a Podman service over its
  API socket.
- `install.docker` — Installs `/usr/bin/docker`, a shim that runs `podman`, and
  `/etc/profile.d/podman-docker.sh`, which, when `DOCKER_HOST` is unset, points it at the user's
  API socket under `$XDG_RUNTIME_DIR`, or at `/run/podman/podman.sock` for root.

## Notes

The install targets write files for systemd whatever the directory variables say:
`install.bin` links `quadlet` into `lib/systemd/system-generators` and `user-generators` and adds
a `tmpfiles.d` entry, and `install.docker` adds tmpfiles entries and a csh profile. The recipe
deletes `usr/lib/systemd`, `usr/lib/tmpfiles.d`, `usr/share/user-tmpfiles.d` and
`podman-docker.csh` afterwards, as nothing in KDOS reads them and no csh is installed. `quadlet`
itself stays in `/usr/lib/podman`, with nothing to run it as a generator, so Quadlet `.container`
files do nothing on KDOS.

There is no socket activation without systemd, so the recipe also writes `kdos-podman-api`, a
small script installed to `/usr/bin`. The *Containers* menu entries, `podman-tui` and
`lazydocker`, run their tool through it: it starts `podman system service` with a five-minute
idle timeout when nothing answers on this user's socket (`/run/podman/podman.sock` for root),
exports `DOCKER_HOST`, and gives `podman-tui` a default connection through
`CONTAINERS_CONF_OVERRIDE` unless the caller set one. The service is started under a lock taken on
a descriptor that is closed for the service itself: the service inherits every open descriptor,
and one that held the lock would block every later launch until it exited.

Rootless Podman puts a container's cgroup beside the session's, and a `--memory` or `--cpus` limit
does nothing without a delegated subtree; how KDOS provides one is in
[Principles](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd).

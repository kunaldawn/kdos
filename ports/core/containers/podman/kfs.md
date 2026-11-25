## About

Podman is a container engine that needs no daemon and runs as an ordinary user. In KDOS it is what
a [box]{.idx} is: every catalogue application runs in a rootless Podman container of its own,
which [kdos-appbox](#port-kdos-appbox) creates (through `distrobox create` for a catalogue
image, `podman create --rootfs` for a pack), starts with `podman start` and enters with
`podman exec`, and the catalogue's images are built with `podman build` on the machine that
installs them. [distrobox](#port-distrobox), [podman-tui](#port-podman-tui) and
[lazydocker](#port-lazydocker) sit on top of it. The launch path is in
[kdos-appbox](../../../../docs/kdos/04-programs/kdos-appbox.md#the-launch-path).

The port builds upstream's Go makefile with a set of build tags of its own, installs the targets one by
one instead of upstream's `install`, which would add systemd units, and removes what systemd alone
reads, with a patch that lets a build tag leave the systemd-only commands out of the binary. Its configuration in `/etc/containers` belongs to
[containers-common](#port-containers-common), not to this package, because buildah and skopeo
read it too; the settings are in
[Containers](../../../../docs/kdos/06-reference/configuration.md#containers).

## Flags

- `BUILDTAGS` — Passed on the `make` command line, so it replaces upstream's whole list. That
  list is `grpcnotrace`, `seccomp` on Linux, and whatever the `hack/*_tag.sh` scripts print after
  probing the build machine: `apparmor`, `systemd`, `libsubid`, `libsqlite3` and the btrfs
  exclusion. Only the four tags below are set, whatever the build root holds; in particular there
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
- `exclude_systemd_commands` — Leaves `podman generate systemd`, `podman quadlet` and
  `podman auto-update` out of the binary: the first two write or manage systemd units, which
  nothing on KDOS reads, and the third restarts containers through the systemd units that run
  them. It also makes `podman generate --help` name Kubernetes YAML alone as what the command
  produces. The tag is not upstream's: it comes from the patch below, and without the patch it
  would be accepted and do nothing.
- `REMOTETAGS` — The tag list `podman-remote` is built with, which `BUILDTAGS` does not reach.
  It is upstream's default (`remote`, `containers_image_openpgp`, the btrfs exclusion) with
  `exclude_systemd_commands` added, so `podman-remote` leaves out the same three commands.
- `GOFLAGS="$GOFLAGS \"-ldflags=-s -w\""` — Extends the tree's exported `GOFLAGS` with `-s -w`,
  which drop the symbol table and DWARF, for any binary the makefile builds without an
  `-ldflags` of its own. See [go](../../../../docs/kdos/05-developer/writing-ports.md#go).
- `EXTRA_LDFLAGS="-s -w"` — The same two flags for the binaries the makefile does link with its
  own `-ldflags`, which a `GOFLAGS` `-ldflags` cannot reach; upstream appends `EXTRA_LDFLAGS` to
  that string.
- `LIBEXECPODMAN=/usr/lib/podman` — Installs the helpers, `rootlessport` among them, in
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

## Patches

The patch adds the `exclude_systemd_commands` build tag: a `//go:build !exclude_systemd_commands`
constraint on `cmd/podman/generate/systemd.go` and on `cmd/podman/auto-update.go`, and the import
that registers `podman quadlet` moved from `cmd/podman/main.go` into a new
`cmd/podman/main_quadlet.go` under the same constraint. `podman auto-update` restarts each
container through the systemd unit its `PODMAN_SYSTEMD_UNIT` label names, so on KDOS it can only
fail; nothing else in package `main` refers to the file's symbols. The description of
`podman generate` takes its examples from a constant defined twice in package `generate`:
`generate_examples_systemd.go`, under `!exclude_systemd_commands`, names "Kubernetes YAML or
systemd units", and `generate_examples.go`, under `exclude_systemd_commands`, names "Kubernetes
YAML". Upstream 6.1.2 has no tag or flag for any of the three commands. Its documentation hunks
are not behind the tag: they drop the `systemd` row from `podman-generate(1)`'s command table, and
the `podman-auto-update` and `podman-quadlet` rows, the Quadlet paragraph and the two Quadlet
entries under SEE ALSO from `podman(1)`, so the installed pages do not list commands the binary
lacks. The tag is opt-in and upstream's default binary is unchanged; the documentation hunks change
the manual pages in every build. The code hunks are to be offered upstream; the documentation
hunks are KDOS's own and are not part of that offer.

## Notes

The install targets write files for systemd whatever the directory variables say:
`install.bin` links `quadlet` into `lib/systemd/system-generators` and `user-generators` and adds
a `tmpfiles.d` entry, and `install.docker` adds tmpfiles entries and a csh profile. The recipe
deletes `usr/lib/systemd`, `usr/lib/tmpfiles.d`, `usr/share/user-tmpfiles.d` and
`podman-docker.csh` afterwards, as nothing in KDOS reads them and no csh is installed. It also
deletes the `quadlet` helper from `/usr/lib/podman`: it is a systemd generator, nothing on KDOS
runs it, and with `podman quadlet` not built nothing else calls it, so Quadlet `.container` files
do nothing on KDOS. `podman generate systemd`, `podman quadlet` and `podman auto-update` are not
built (see `exclude_systemd_commands`). It deletes `/usr/bin/podmansh`, the link to `podman` that
`install.bin` makes, with `podmansh.1`: as a login shell it only enters a `podmansh` container,
which a Quadlet under systemd creates, so on KDOS it has nothing to enter. For the same reason the recipe deletes the manual pages that
document Quadlet and systemd units: `podman-systemd.unit.5`, `quadlet.5`, the seven
`podman-{build,container,image,kube,network,pod,volume}.unit.5`, `podman-quadlet.1` with its
`-install`, `-list`, `-print` and `-rm` pages, `podman-generate-systemd.1`,
`podman-auto-update.1`, which documents a command that needs systemd units, and the guide
`podman-quadlet-basic-usage.7`. Left in, `apropos systemd` and `apropos quadlet` would send a
reader to them. `podman-rootless.7` and `podman-troubleshooting.7` stay, the latter with its
section *Cannot enable a quadlet unit* as upstream wrote it, and so do the passing Quadlet
mentions in `podman-pod-create.1` and `podman-system-migrate.1`. Pages other than `podman.1` that
stay, `podman-run.1` and `podman-create.1` among them, still name the deleted ones under SEE ALSO,
and `man` reports those as missing.

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
[Principles](../../../../docs/kdos/01-philosophy/principles.md#no-systemd).

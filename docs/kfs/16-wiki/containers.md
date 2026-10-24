# Containers

KDOS ships a [rootless]{.idx term="rootless containers"} [container engine]{.idx}, Podman, with the tools around it: distrobox for
interactive [containers]{.idx term="container"} that share your home, buildah and skopeo for building and copying images,
and two terminal programs that show what is running. The [boxes]{.idx term="box"} the desktop's applications run in
are Podman containers too, managed by [`kdos-box`]{.idx} ([Boxes](boxes.md)); this page is about using the
engine directly. It covers running a container, the subordinate ids and [cgroups]{.idx term="cgroup"} a rootless
container needs, the storage driver, building images, the Docker API socket that some tools want,
and what to check when a container will not start.

## Installation

The engine and its tools are ports on the `containers` shelf:

| Port | What it installs |
|---|---|
| [podman](#port-podman) | [`podman`]{.idx}, `podman-remote`, a `docker` command that runs `podman`, the helpers in [`/usr/lib/podman`]{.idx}, and [`kdos-podman-api`]{.idx} |
| [containers-common](#port-containers-common) | Everything under [`/etc/containers`]{.idx}, which podman, buildah and skopeo all read |
| [crun](#port-crun) | The OCI runtime that starts a container's processes |
| [conmon](#port-conmon) | The monitor process each running container has |
| [netavark](#port-netavark), [aardvark-dns](#port-aardvark-dns) | Container networks and their name server |
| [passt](#port-passt) | `pasta`, the user-mode network of a rootless container |
| [slirp4netns](#port-slirp4netns) | The other user-mode network, for a container started with `--network slirp4netns` |
| [fuse-overlayfs](#port-fuse-overlayfs) | The [overlay]{.idx} driver in user space, used on the live medium |
| [catatonit](#port-catatonit) | The small init behind `--init` and a pod's infra container |
| [distrobox](#port-distrobox) | `distrobox`: containers that share your home and look like part of the host |
| [buildah](#port-buildah), [skopeo](#port-skopeo) | Building images, and inspecting and copying them between registries |
| [podman-tui](#port-podman-tui), [lazydocker](#port-lazydocker) | The two *Containers* entries in the menu |

[`newuidmap`]{.idx} and [`newgidmap`]{.idx}, the setuid helpers that map a range of ids into a rootless container,
come from [shadow](#port-shadow). The base filesystem supplies [`/etc/subuid`]{.idx} and [`/etc/subgid`]{.idx},
and the service `userdirs` (`/etc/init.d/15_userdirs.sh`) delegates a cgroup subtree to each
account at boot.

Podman has no daemon, so there is no container service to enable. The API socket that Docker
clients use is started on demand by `kdos-podman-api`.

## Configuration

### /etc/containers

All four files are written by the [containers-common](#port-containers-common) port's `build.sh`:

<!-- kfs:include ports/core/containers/containers-common/build.sh lines=14-51 pin=15f5884f2859 -->

The cgroup manager is `cgroupfs`, because the other one Podman offers, `systemd`, needs systemd.
`storage.conf` names the driver and its mount program and no store path, so root's store is the
engine's default, `/var/lib/containers/storage`, and yours is under your home. Rootless Podman
reads a `graphroot` or `runroot` from this file as well, and with root's paths in it every
container an ordinary account starts fails with
`mkdir /var/lib/containers/storage: permission denied`.
`policy.json` accepts any image and checks no signature. A short image name such as `alpine` is
looked up on `docker.io` alone. What each setting means is in
[Containers](../../kdos/06-reference/configuration.md#containers).

### /etc/subuid and /etc/subgid

A rootless container maps a range of subordinate ids, and each account needs one. The base
filesystem's files, `fs/etc/subuid` and `fs/etc/subgid`, are the same:

<!-- kfs:include fs/etc/subuid -->

The installer renames the `kdos` entries to the account you create. `useradd` gives a new account a
range of its own, as `SUB_UID_MIN`, `SUB_UID_COUNT` and their `SUB_GID_` counterparts in
`/etc/login.defs` say.

### The delegated cgroup

Rootless Podman applies `--memory`, `--cpus` and `--pids-limit` through a cgroup subtree it owns.
`15_userdirs.sh` makes one for each account at boot:

<!-- kfs:include fs/etc/init.d/15_userdirs.sh lines=30-48 pin=3739f683a6a6 -->

`kdos-getty` moves an [autologin]{.idx} session into `session/`, so containers started from it get limits
the kernel enforces. A session started any other way, a password login on a virtual terminal or an
ssh login, stays in the root cgroup, where Podman accepts a limit and nothing applies it.

### ~/.config/containers/storage.conf

Your own store is `~/.local/share/containers/storage`, and its run directory
`$XDG_RUNTIME_DIR/containers`; `podman info --format '{{.Store.GraphRoot}}'` prints the first.
[`/etc/containers/storage.conf`]{.idx} mounts it through [`fuse-overlayfs`]{.idx}, which the live
medium needs, because its home directory is on the boot
overlay and the kernel will not stack an overlay's writable layer on an overlay. On an installed
system, `kdos-appbox` writes `~/.config/containers/storage.conf` on its first run, choosing the
kernel's own overlay driver, but only when the file does not exist, the store holds no container,
and your home is on ext2, ext3, ext4, btrfs, XFS or F2FS. The two drivers mark deletions
differently, so once a container exists the driver must not change. See
[Storage drivers](../../kdos/04-programs/kdos-appbox.md#storage-drivers).

### kdos-podman-api

Without systemd there is no socket activation, so the [podman](#port-podman) port writes a small
script that starts `podman system service` when nothing answers on your socket,
`$XDG_RUNTIME_DIR/podman/podman.sock` (`/run/podman/podman.sock` for root), and runs a command
against it. The service exits after five minutes with no client:

<!-- kfs:include ports/core/containers/podman/build.sh lines=35-93 pin=2f2d1e86bdb2 -->

The *Containers* menu entry runs [`podman-tui`]{.idx} through it:

<!-- kfs:include ports/core/containers/podman-tui/build.sh lines=20-31 pin=e3f88e31e447 -->

## Usage

### Running a container

1. Run one, removed again when it exits:

   ```bash
   podman run --rm -it docker.io/library/alpine:latest sh
   ```

2. List what exists, running or not, and the images:

   ```bash
   podman ps -a
   podman images
   ```

3. Stop and remove a container, and remove an image:

   ```bash
   podman stop <name>
   podman rm <name>
   podman rmi <image>
   ```

`podman ps -a` also lists the boxes. Leave those to `kdos-box`, which keeps their profiles and
composed roots in step: removing one with `podman rm` leaves its profile and its pack mounts behind.

### A distrobox

distrobox makes a container that shares your home directory, your user and the host's display, so
programs in it feel installed:

```bash
distrobox create --name dev --image docker.io/library/debian:stable
distrobox enter dev
```

The first `enter` sets the container up (`Container Setup Complete!`); inside it `$HOME` is
`/home/<you>` and `id -un` is your own name. `distrobox enter dev -- <command>` runs one command
and returns. `distrobox rm -f dev` can wait ten seconds on a container that ignores podman's stop
signal and then report that it could not be stopped; run it again.

The boxes `kdos-box create <name> base=image:<ref>` makes are distrobox containers, with a profile
and the desktop's launchers; see [Boxes](boxes.md#a-box-of-your-own).

### Building and copying images

```bash
podman build -t localhost/mytool -f Containerfile .
buildah bud -t localhost/mytool -f Containerfile .
skopeo inspect docker://docker.io/library/alpine:latest
skopeo copy docker://docker.io/library/alpine:latest oci-archive:alpine.tar
```

### Programs that speak the Docker API

`podman-tui`, [`lazydocker`]{.idx} and other Docker clients talk to a service over a socket. Run them through
`kdos-podman-api`, which starts the service when it is not running and sets `DOCKER_HOST`:

```bash
kdos-podman-api lazydocker
kdos-podman-api podman-tui
```

The menu's *Containers* and *Containers (lazydocker)* entries do this. `kdos-podman-api` gives
`podman-tui` its connection by writing a small containers configuration that names the socket,
`$XDG_RUNTIME_DIR/podman/kdos-api.conf`, and pointing `CONTAINERS_CONF_OVERRIDE` at it; when you have exported a `CONTAINERS_CONF_OVERRIDE` of
your own, it leaves yours in place and sets only `DOCKER_HOST`. Any command works in their place,
which is the quickest check that the service starts:

```bash
kdos-podman-api sh -c 'podman-remote --url "$DOCKER_HOST" version'
```

### Limiting a container

```bash
podman run --rm -it --memory 2g --cpus 2 docker.io/library/alpine:latest sh
cat /sys/fs/cgroup/user.slice/user-$(id -u)/cgroup.subtree_control
```

The limit is applied only in a session `kdos-getty` placed in the delegated cgroup, which is the
autologin session; check which cgroup your shell is in with `cat /proc/self/cgroup`. A terminal
in the autologin desktop prints `0::/user.slice/user-1000/session`; a shell from `su - <you>` or
a password login prints `0::/`, and there a `--memory` container has no `memory.max` file at all.

### On the live medium and on an installed system

On the live medium the store is on the overlay in RAM, goes through `fuse-overlayfs`, and is gone
at power-off unless the medium has a persistence store. On an installed system the store is on
disk, and uses the kernel's overlay driver once `kdos-appbox` has written your `storage.conf`.

## Tips

- `docker` is a script that runs `podman` with the same arguments. It prints
  `Emulate Docker CLI using podman. Create /etc/containers/nodocker to quiet msg.` on each run
  until [`/etc/containers/nodocker`]{.idx} or `~/.config/containers/nodocker` exists.
- `podman run --init` and pods use `catatonit`, at `/usr/lib/podman/catatonit`; inside the
  container it is process 1, named `/run/podman-init`.
- Quadlet is installed, in `/usr/lib/podman`, and nothing runs it: there is no systemd to read
  `.container` files.
- An image pulled by a short name comes from `docker.io`; give the full reference
  (`quay.io/...`, `ghcr.io/...`) for any other registry.
- `podman system df` shows how much space images, containers and volumes take, and
  `podman system prune` removes what nothing uses. It removes stopped boxes too, so run it with
  care on a desktop with applications installed.

## Troubleshooting

Start with `kdos doctor`. Its Containers section checks that the mount namespace's root is `/` and
that `/etc/subuid` and `/etc/subgid` carry your range; its Desktop section checks the setuid bits
on `newuidmap` and `newgidmap`. On a healthy machine:

```text
Containers
  [ ok ] mount namespace root is /
  [ ok ] subuid mapping present
  [ ok ] subgid mapping present
```

`Error: creating runtime static files directory "/var/lib/containers/storage/libpod": mkdir /var/lib/containers/storage: permission denied`
: Rootless Podman is using root's store, because a `storage.conf` it reads names `graphroot` or
  `runroot`. `podman info --format '{{.Store.GraphRoot}}'` should print a path under your home.
  `/etc/containers/storage.conf` as shipped names neither; remove the two lines from it, or from
  `~/.config/containers/storage.conf`. `kdos doctor` does not check this.

Every container fails, the engine exits 125
: `newuidmap` or `newgidmap` has lost its setuid bit:
  `sudo chmod 4755 /usr/bin/newuidmap /usr/bin/newgidmap`.

`no subuid ranges found for user`
: Your account has no line in `/etc/subuid` or `/etc/subgid`. Add a range that overlaps no other
  account's, then run `podman system migrate`:

  ```bash
  sudo usermod --add-subuids 300000-365535 --add-subgids 300000-365535 "$USER"
  podman system migrate
  ```

A container fails to mount its root on the live medium
: A `~/.config/containers/storage.conf` names the kernel's overlay driver, which cannot stack on
  the boot overlay. Move the file aside, and the system file's `fuse-overlayfs` setting applies.
  Containers made under the other driver cannot be used with this one.

A `--memory` or `--cpus` limit has no effect
: The session is not in the delegated cgroup. `cat /proc/self/cgroup` prints `0::/` in the root
  cgroup; the limits apply only from an autologin session.

`podman-tui` or `lazydocker` shows nothing
: Run it through `kdos-podman-api`, which starts the API service. When that fails it says
  `nothing answers on <socket>`; run `podman system service --time=0` in a terminal to see why.

An image pull refuses to start
: [`/etc/containers/policy.json`]{.idx} is missing; reinstall [containers-common](#port-containers-common)
  with `sudo kpkg install -f containers-common` (see [kpkg](kpkg.md)).

## Known gaps

- [The *Containers* menu entries have not been opened on a built image](../../kdos/06-reference/known-gaps.md#the-containers-menu-entries-have-not-been-opened-on-a-built-image)
- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
- [A box is not a security boundary against you](../../kdos/06-reference/known-gaps.md#a-box-is-not-a-security-boundary-against-you)
- [Some command-line programs build after Qt](../../kdos/06-reference/known-gaps.md#some-command-line-programs-build-after-qt): podman and distrobox build in `43_toolkits`, so a build that stops before it has neither.

## See also

- [Containers](../../kdos/03-architecture/security-model.md#containers) in the security model: rootless containers, profiles, and what a pulled image is trusted for
- [Containers](../../kdos/06-reference/configuration.md#containers) in the configuration reference
- [Storage drivers](../../kdos/04-programs/kdos-appbox.md#storage-drivers): the driver choice and why it must not change
- [Principles: no systemd](../../kdos/01-philosophy/principles.md#no-systemd): what replaces systemd's user slice
- [Boxes](boxes.md), [Packs](packs.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 409.* Previous: [408. Boxes](boxes.md) · [Contents](../README.md) · Next: [410. Virtual machines](virtual-machines.md)

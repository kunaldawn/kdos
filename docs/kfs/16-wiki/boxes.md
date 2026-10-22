# Boxes

A [box]{.idx} is the rootless Podman container one graphical application, or one working
environment, runs in. The applications KDOS does not compile, the ones the catalogue lists, each
run in a box of their own named after them, and look native: a [launcher]{.idx} in the menu, a command on
the `PATH`, files opening in them. This page covers installing and removing a catalogue
application, launching it, managing its box, changing what a box may share with the host, taking
and restoring snapshots, and making a box of your own to work in. Carrying applications to another
machine is [Packs](packs.md); Podman and distrobox used directly are [Containers](containers.md).

## Installation

- [kdos-appbox](#port-kdos-appbox) installs `/usr/local/bin/kdos-appbox` and, as links to it,
  `kdos-box` and `xdg-open`, and the catalogue, [`/usr/share/kdos/appstore/catalogue`]{.idx}. It depends
  on [podman](#port-podman) and [distrobox](#port-distrobox).
- `kdos app`, the everyday command, is in [kdos-tools](#port-kdos-tools); every verb of it that
  builds, removes, exports or imports runs `kdos-appbox`.
- [kdos-boxinit](#port-kdos-boxinit) is process 1 inside a box made from packs, and
  [kdos-boxsock](#port-kdos-boxsock) holds each box's own tagged Wayland socket.
- [kdos-packd](#port-kdos-packd), the service `packd`, mounts and composes the packs a pack box is
  made of; see [Packs](packs.md).
- The desktop session starts the boxes behind your pinned launchers at login and stops idle ones
  every ten minutes, from `/usr/local/lib/kdos/session-common.sh`.

The image carries the catalogue and no application: each one is built on the machine that asks for
it, which needs the network. Boxes need no service of their own.

## Configuration

### The catalogue

`/usr/share/kdos/appstore/catalogue` lists every application KDOS knows how to build, as a row
naming its parent row and the Debian packages it adds. `kdos app`, the store window and the
installer all read this one file. Its `snapshot` line pins the Debian archive date the packages
come from:

<!-- kfs:include src/system/kdos-appbox/catalogue lines=34-38 pin=3dc0c4b7b7fa -->

An application row and its description, as an example:

<!-- kfs:include src/system/kdos-appbox/catalogue lines=314-314 pin=db241a9ade70 -->

<!-- kfs:include src/system/kdos-appbox/catalogue lines=502-502 pin=fdc887213656 -->

The format of every line kind is in
[The catalogue file](../../kdos/04-programs/kdos-appbox.md#the-catalogue-file).
`KDOS_CATALOGUE` names another catalogue to read instead.

### Box profiles

A box's settings are `~/.config/kdos/boxes/<name>.conf`, flat `key = value` lines. None is shipped:
`kdos-box create` and `kdos-box profile <name> key=value` write one with every key spelled out,
and a box with no file behaves as one with the defaults. The keys say what the box shares with the
host (`home`, `network`, `ipc`, `devices`, `processes`), what it may use (`gpu`, `render`,
`memory`, `cpus`, `pids`, `grant`) and how it is presented (`accent`, `autostop`). Every key, its
default and what enforces it are in
[Box profiles](../../kdos/04-programs/kdos-appbox.md#box-profiles) and
[the box profile file](../../kdos/06-reference/configuration.md#configkdosboxesnameconf).
The compositor reads the same file for the box's `grant` line.

### What the session starts

The session's `kdos_session_boxes`, in `fs/usr/local/lib/kdos/session-common.sh`, runs two
commands in the background. The warmup, once at login:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=91-91 pin=fa4c063a4b0e -->

and the idle sweep, every ten minutes from login on. The loop keeps running after Log Out and ends
with the login; a box stops for neither:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=99-99 pin=b0b7bb100465 -->

The warmup starts up to eight pack boxes, the ones behind the entries of
`~/.config/kdos/favorites`, in order. Its skeleton, `fs/etc/skel/.config/kdos/favorites`, describes
the format:

<!-- kfs:include fs/etc/skel/.config/kdos/favorites lines=10-20 pin=625f093219e1 -->

### Other files

| Path | What |
|---|---|
| `~/.local/share/kdos/boxes/<name>/` | A pack box's writable layer (`upper`), its snapshots, and a private home |
| `~/.local/share/kdos/alien-apps`, `/usr/share/kdos/alien-apps` | The tables that map an application's command to its box |
| `~/.local/share/applications/` | The launchers the regeneration writes, each marked `X-KDOS-Alien=true` |
| `~/.config/kdos/a11y` | When it exists, boxes are started with accessibility on |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | How long each stage of the last launches took |

The rest are in [Files and variables](../../kdos/04-programs/kdos-appbox.md#files-and-variables).

## Usage

### Finding an application

```bash
kdos app list --all            # the whole catalogue
kdos app search panorama       # by id, name, category or tagline
kdos app info app.hugin        # one entry, its state and its size estimate
kdos app groups                # the named groups install takes in place of an id
kdos app list                  # what is installed
```

`kdos app list --all` prints a table of id, category, state, estimated size and tagline, one row
per application and data pack. `kdos app info app.hugin` names the runtime row the application is
`built on` (`rt-gtk`). On a fresh machine `kdos app list` answers
``nothing installed — `kdos app list --all` ``.

### Installing an application

1. Preview what would be built. It prints the `Containerfile` of each image in the stack, under
   `--- kdos/base`, `--- kdos/rt-gtk` and `--- kdos/app.hugin`, and builds nothing:

   ```bash
   kdos app install app.hugin --dry-run
   ```

   With the catalogue's `snapshot = auto`, the dry run also warns
   `could not read the snapshot date out of debian:trixie-slim — building against the live archive,
   which is not reproducible` when the base image has not been pulled yet.

2. Install it, by id or by group. It builds a stack of images on this machine, one per catalogue
   row, `kdos/base`, then the runtime, then the application; an image already built is skipped:

   ```bash
   kdos app install app.hugin
   ```

3. Read the result. Progress is one line per step (`==> building kdos/app.hugin`); the exit status
   is the number of applications that failed.

The step after the images, creating the box with `kdos-box create <id> base=image:kdos/<id>`,
fails as shipped: that create runs `podman pull` on the image's name, which asks a registry at
`localhost` for the image it has built, and none answers. With a local image of that name in
place, the create prints podman's
`pinging container registry localhost: Get "https://localhost/v2/": … connection refused` three
times, then `kdos-appbox: could not fetch kdos/<id>`. The install command prints
`<id>: the image built and the box did not`, and no launcher is made. See
[install and uninstall](../../kdos/04-programs/kdos-appbox.md#install-and-uninstall).

The images stay built, and `kdos app export` packs only built images, so the pack route can take
over from here: export the application to a set and import the set, as [Packs](packs.md) shows,
and the import creates a box on the packs. Each half is documented; the two have not been run
one after the other.

### Launching

Every installed application is a launcher in the menu and a command:

```bash
hugin shots.pto                        # the shim on the PATH
kdos app launch app.hugin              # installs first if it must
kdos-appbox run hugin shots.pto        # the launch path itself
kdos-appbox open report.pdf            # whatever handles the file, boxed or native
```

A launch starts the box if it is stopped, and composes a pack box's root first.

### Managing boxes

```bash
kdos-box list                          # every box: base, state, persistence, disk use, accent
kdos-box enter app.hugin               # a shell inside it, in a new terminal window
kdos-box enter app.hugin ls /usr/bin   # one command, in place
kdos-box stop app.hugin                # podman stop -t 10: waits 10 seconds, then kills
kdos-box start app.hugin
kdos-box remove app.hugin              # the profile and ~/.local/share/kdos/boxes/<name> stay
```

`kdos-box list` prints one row per box under `BOX BASE STATE PERSIST DISK ACCENT`; a box whose
profile exists and whose container does not is `not created`. A box made from an image, a distrobox
container, does not exit on podman's stop signal, so its `stop` takes the whole ten seconds and
the kill (15 seconds on the live medium), and `list` may still show it `stopping` for a moment.

`remove` runs `podman rm` without `-f`, which refuses a running box. On success it prints
`<name> removed — its profile and its writable layer are still in ~/.local/share/kdos/boxes/<name>`
and exits 0. When podman refuses, podman's own error is printed, then
`<name> was not removed (podman rm exited N)`, and `kdos-box remove` exits with podman's status;
the box and its stack are left as they were. Stop the box first with `kdos-box stop`, then remove
it. `--force` (`kdos-box remove app.hugin --force`) adds `-f` to `podman rm`, which does not
remove a running box made from an image on the live medium: podman gives up with `given PID did
not die within timeout`, the box is left `stopping`, and `kdos-box remove` reports the failure
the same way. A name with no container answers `<name> has no container to remove` and exits 1, after
releasing any composed stack that a failed `create` left behind.

`kdos-box enter` with no command, from a terminal, opens a new `foot` window titled
`<name> — KDOS box`; with `KDOS_BOX_NOTERM` set to anything it runs in the current terminal.
Inside, `KDOS_BOX` holds the box's name.

### Changing what a box may share

1. Print the profile. Each key is shown beside the mechanism that enforces it, and a `!` line
   marks a setting it cannot deliver:

   ```bash
   kdos-box profile app.hugin
   ```

2. Set keys. It answers `<name>: written`, reminds you that namespaces and volumes apply at
   create time, and prints the profile again; `autostop=30m` is stored as `autostop=1800s`:

   ```bash
   kdos-box profile app.hugin home=private network=none memory=4G
   ```

3. Namespaces and shared directories are set when a box is created. For those keys, remove the
   box and let it be created again from the kept profile: a pack box is recreated by its next
   launch.

   ```bash
   kdos-box remove app.hugin
   ```

`memory` and `cpus` become [cgroup]{.idx} limits only in a session started by autologin, which
`kdos-getty` puts in your delegated cgroup; in a session started any other way the engine accepts
them and the kernel does not apply them. See
[memory and grants](../../kdos/04-programs/kdos-appbox.md#memory-and-grants).

### Granting a box more of the desktop

A box's clients reach the compositor through a tagged socket, and the compositor refuses them
screen capture, clipboard managers, layer-shell surfaces and input methods unless the profile
grants them by name:

```bash
kdos-box profile app.hugin grant=screencopy
kdos-comp -r
```

The names are `screencopy`, `toplevel-capture`, `export-dmabuf`, `data-control`,
`foreign-toplevel`, `layer-shell`, `input-method` and `output-power`. The compositor reads a box's
`grant` line once and keeps the answer until it reloads its configuration, which `kdos-comp -r`
asks for; after that the box's next client sees the change, and a running one keeps what it bound.
`input-method` sees every key typed on the seat. See
[Granting a box past the allowlist](../../kdos/03-architecture/security-model.md#granting-a-box-past-the-allowlist).

### Snapshots, clones and rollback

These work on a pack box's writable layer, under `~/.local/share/kdos/boxes/<name>/`. A box built
from a container image keeps its writes inside the engine's store, where they do not reach.

```bash
kdos-box snapshot app.scribus before-plugin    # a full copy of the writable layer, tagged
kdos-box snapshots app.scribus                 # list them, with sizes
kdos-box stop app.scribus
kdos-box rollback app.scribus before-plugin    # refused while the box runs
kdos-box clone app.scribus scribus-test        # a new box on the same base, the work copied
```

A snapshot costs as much disk as the box has written. Without a tag it is named by the UTC time.

### A box of your own

A box made from a container image is a working environment, not tied to an application:

1. Create it. An `image:` base pulls from a registry over the network, and prints that the content
   is unsigned:

   ```bash
   kdos-box create scratch base=image:docker.io/library/alpine:latest
   ```

   distrobox reports `Distrobox 'scratch' successfully created.`, and the command ends with
   `scratch created` and the new profile. `kdos-box list` then shows the box `created`.

2. Enter it. With a command it runs in place, which is also how to check it:

   ```bash
   kdos-box enter scratch
   kdos-box enter scratch cat /etc/alpine-release
   ```

3. Put one of its graphical programs in the menu, as `<app> (scratch)`:

   ```bash
   kdos-box apps scratch
   kdos-box export scratch <desktop-id>
   ```

   `kdos-box apps` lists the desktop ids in the box's `/usr/share/applications`, and prints
   nothing for an image, such as Alpine's, that has none. `export` writes
   `~/.local/share/applications/<desktop-id>.scratch.desktop`, which works, and a shim
   `~/.local/bin/<desktop-id>@scratch`, which answers `unknown alien app` until a table row of that
   name exists. `kdos-box unexport scratch <desktop-id>` removes both.

A box on a pack base (`base=pack:<id>`) needs no network, and its pack was verified when it was
installed. Do not use `base=box:<name>`: that create never finishes, repeating the same step until
it is interrupted; read the other box's base with
`kdos-box profile <name>` and pass it instead, or use `kdos-box clone`.

### Stopping idle boxes

A box warmed at login keeps running until the machine shuts down, past Log Out and past the end of
the login, unless its profile sets `autostop`:

```bash
kdos-box profile app.hugin autostop=30m
kdos-box gc --dry-run                  # what the next sweep would stop
```

With nothing past its time, `gc --dry-run` prints `nothing idle`.

The session runs `kdos-box gc` every ten minutes, from login until the login ends, so a box left
running after the login has ended waits for the next login's sweep. It stops a box that has run longer than its
`autostop` time, counted from when it started, and only after the compositor says the box has no
window open.

### Removing an application

```bash
kdos app remove app.hugin
```

It removes the box, the application's image, and each runtime image above the base that no
remaining box's chain names, then regenerates the launchers. The base image is never removed.

### Updating an application

There is no update verb. `kdos app remove` and then `kdos app install` rebuild the application's
image, and the box creation after it fails as [Installing an application](#installing-an-application)
says; see [Updating](updating.md#updating-boxed-applications).

### On the live medium and on an installed system

On the live medium the home directory is on the boot overlay, and the kernel will not stack a box's
writable layer on it. A pack box's writes then go to a tmpfs under `/run/user/<uid>/kdos/boxes/`
and end with the session, `kdos-appbox status` marks the box `ephemeral`, and `kdos doctor` warns. The
container engine's own store uses `fuse-overlayfs` there; on an installed system the first run
switches your store to the kernel's overlay driver (see [Containers](containers.md)). Everything
built on the live medium is in RAM unless the medium has a persistence store.

## Tips

- `render` chooses who draws: `auto` uses the card when a `/dev/dri/renderD*` node opens for you,
  `software` forces `LIBGL_ALWAYS_SOFTWARE=1`. Under QEMU without virgl, `auto` resolves to
  software.
- `touch ~/.config/kdos/a11y` turns accessibility on in every box; `KDOS_A11Y=1` does it for one
  launch.
- A store install that fails part-way keeps what it built: of six applications where the fourth
  fails, the other five are installed.
- A box shares your whole home directory by default. `home=private` gives a box one of its own, at
  `~/.local/share/kdos/boxes/<name>`.

## Troubleshooting

Start with:

```bash
kdos doctor
kdos-appbox status
```

`kdos doctor`'s Boxes section checks EROFS, the pack daemon, that you are uid 1000, and whether your
home can hold a box's writable layer; its Containers and Desktop sections check `/etc/subuid`,
`/etc/subgid` and the setuid bits on [`newuidmap`]{.idx} and [`newgidmap`]{.idx}.

`<id>: the image built and the box did not`
: The box create after a store install fails as shipped; see
  [Installing an application](#installing-an-application).

A launch does nothing
: Run it from a terminal with the engine's messages shown: `kdos-appbox -v run <app>`. The
  stage timings of recent launches are in `$XDG_RUNTIME_DIR/kdos-appbox.trace`.

`could not start <name>`
: `enter` and `start` do not show the engine's message. Look at the container with
  `podman ps -a` and `podman logs <name>`, or in the *Containers* terminal program, `podman-tui`.

No box starts, and the engine exits 125
: `newuidmap` or `newgidmap` has lost its setuid bit. `kdos doctor` names it; the fix is
  `sudo chmod 4755 /usr/bin/newuidmap /usr/bin/newgidmap`.

`mkdir /var/lib/containers/storage: permission denied` on every box command
: The engine is using root's store: `/etc/containers/storage.conf`, or your own, names a
  `graphroot` or `runroot`. See [Containers](containers.md#troubleshooting).

A box's work is gone after a reboot
: It ran on the live medium, or `kdos-appbox status` marks it `ephemeral`; see
  [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box).

A profile change has no effect
: Namespace and directory keys apply at creation. Remove the box and launch the application again.
  A `grant` change needs `kdos-comp -r`, and then reaches the next client, not a running one.

`kdos-box snapshot` says `<name> has nothing written to snapshot`
: The box was built from a container image, and its writes are in the engine's store.

## Known gaps

- [A box can be granted only the eight named groups of Wayland globals](../../kdos/06-reference/known-gaps.md#a-box-can-be-granted-only-the-eight-named-groups-of-wayland-globals)
- [A boxed application is given your location without being asked](../../kdos/06-reference/known-gaps.md#a-boxed-application-is-given-your-location-without-being-asked)
- [A catalogue change reaches an installed application only when it is rebuilt](../../kdos/06-reference/known-gaps.md#a-catalogue-change-reaches-an-installed-application-only-when-it-is-rebuilt)
- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
- [kdos-box create with a box: base never finishes](../../kdos/06-reference/known-gaps.md#kdos-box-create-with-a-box-base-never-finishes)
- [A box is not a security boundary against you](../../kdos/06-reference/known-gaps.md#a-box-is-not-a-security-boundary-against-you)
- [Changing the accent colour reaches running GTK3 applications, not running libadwaita or KDE ones](../../kdos/06-reference/known-gaps.md#changing-the-accent-colour-reaches-running-gtk3-applications-not-running-libadwaita-or-kde-ones)
- [Applications imported or built during an install stay in the live session](../../kdos/06-reference/known-gaps.md#applications-imported-or-built-during-an-install-stay-in-the-live-session)

## See also

- [Applications](../../kdos/02-user-guide/applications.md): the user guide's chapter, the store and the everyday commands
- [kdos-appbox](../../kdos/04-programs/kdos-appbox.md): every command, the launch path and the profile keys
- [Packs and boxes](../../kdos/03-architecture/packs-and-boxes.md): the two lanes, composition and the box
- [Containers](../../kdos/03-architecture/security-model.md#containers) in the security model: what a box shares and what that means
- [kdos app](../../kdos/04-programs/kdos-command.md#kdos-app)
- [Packs](packs.md), [Containers](containers.md), [Updating](updating.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 408.* Previous: [407. Packs](packs.md) · [Contents](../README.md) · Next: [409. Containers](containers.md)

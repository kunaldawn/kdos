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
  on [podman](#port-podman), [distrobox](#port-distrobox),
  [shared-mime-info](#port-shared-mime-info) and [xdg-utils](#port-xdg-utils).
- `kdos app`, the everyday command, is in [kdos-tools](#port-kdos-tools); every verb of it that
  builds, removes, exports or imports runs `kdos-appbox`.
- [kdos-boxinit](#port-kdos-boxinit) is process 1 inside a box made from packs, and
  [kdos-boxsock](#port-kdos-boxsock) holds each box's own tagged Wayland socket.
- [kdos-packd](#port-kdos-packd), the service `packd`, mounts and composes the packs a pack box is
  made of; see [Packs](packs.md).
- The desktop session starts the boxes behind your pinned launchers at login and, every ten
  minutes, stops any box that has run past its profile's `autostop` time with no window open, from
  `/usr/local/lib/kdos/session-common.sh`.

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

<!-- kfs:include src/system/kdos-appbox/catalogue lines=320-320 pin=db241a9ade70 -->

<!-- kfs:include src/system/kdos-appbox/catalogue lines=508-508 pin=6a389e27c186 -->

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

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=92-92 pin=fa4c063a4b0e -->

and the idle sweep, every ten minutes from login on, started only when the pid in
`$XDG_RUNTIME_DIR/kdos/box-gc.pid` is not already a running loop, so a login has one however many
sessions it starts. The loop keeps running after Log Out and ends with the login; a box stops for
neither:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=109-114 pin=e3ccc5f78bfa -->

The warmup starts up to eight boxes, the ones behind the entries of
`~/.config/kdos/favorites`, in order. Its skeleton, `fs/etc/skel/.config/kdos/favorites`, describes
the format:

<!-- kfs:include fs/etc/skel/.config/kdos/favorites lines=11-19 pin=9d380fc97c16 -->

### Other files

| Path | What |
|---|---|
| `~/.local/share/kdos/boxes/<name>/` | A pack box's writable layer (`upper`), its snapshots, and a private home |
| `~/.local/share/kdos/alien-apps` (`$XDG_DATA_HOME/kdos/alien-apps` when that is set), `/usr/share/kdos/alien-apps` | The tables that map an application's command to its box |
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

   With the catalogue's `snapshot = auto`, the date is read by running `debian:trixie-slim`, which
   pulls that image first when it is not in your image store. When that run fails (no network, or
   podman cannot start a container) or the image's sources file carries no snapshot line, the dry
   run and the install both warn
   `could not read the snapshot date out of debian:trixie-slim — building against the live archive,
   which is not reproducible` and build unpinned.

2. Install it, by id or by group. It builds a stack of images on this machine, one per catalogue
   row, `kdos/base`, then the runtime, then the application; an image already built is skipped:

   ```bash
   kdos app install app.hugin
   ```

3. Read the result. Progress is one line per step (`==> building kdos/app.hugin`); the exit status
   is the number of applications that failed.

The step after the images creates the box with `kdos-box create <id> base=image:kdos/<id>`. The
image is already in your image store, so the create uses it without asking a registry, and the
install then writes the application's launchers. When the create fails, the install prints
`<id>: the image built and the box did not`, no launcher is made, and the images stay built.
Running `kdos app install <id>` again skips those images and tries the box again;
`kdos-box remove <id>` deletes the profile the failed create left. See
[install and uninstall](../../kdos/04-programs/kdos-appbox.md#install-and-uninstall).

`kdos app export` packs only built images, so the pack route can take over from a built image as
well: export the application to a set and import the set, as [Packs](packs.md) shows, and the
import creates a box on the packs.

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
`<name> removed — its profile is kept in ~/.config/kdos/boxes/<name>.conf`, followed by
`and its writable layer, snapshots and home in ~/.local/share/kdos/boxes/<name>` when that
directory exists, and exits 0. A store box has no such directory: its writes were inside its
container and go with it. When podman refuses, podman's own error is printed, then
`<name> was not removed (podman rm exited N)`, which without `--force` goes on to say that a
running box needs `kdos-box stop` first, and `kdos-box remove` exits with podman's status;
the box and its stack are left as they were. Stop the box first with `kdos-box stop`, then remove
it. `--force` (`kdos-box remove app.hugin --force`) adds `-f` to `podman rm`, which does not
remove a running box made from an image on the live medium: podman gives up with `given PID did
not die within timeout`, the box is left `stopping`, and `kdos-box remove` reports the failure
the same way. A box with no container, such as one whose `create` failed, has its profile deleted
and any composed stack released, answers `<name> had no container — its profile is removed` and
exits 0; a name with neither a container nor a profile answers `<name> has no container to remove`
and exits 1, after releasing any composed stack the same way.

`kdos-box enter` with no command, from a terminal, opens a new `foot` window titled
`<name> — KDOS box`; with `KDOS_BOX_NOTERM` set to anything it runs in the current terminal.
Inside, `KDOS_BOX` holds the box's name.

### Changing what a box may share

1. Print the profile. Each key is shown beside the mechanism that enforces it, and a `!` line
   marks a setting it cannot deliver:

   ```bash
   kdos-box profile app.hugin
   ```

2. Set keys. For a box that has a container it answers `<name>: written`, reminds you that
   namespaces and volumes apply at create time, and prints the profile again; for one with no
   container yet it says the profile applies when the box is created. `autostop=30m` is stored as
   `autostop=1800s`:

   ```bash
   kdos-box profile app.hugin home=private network=private
   ```

3. Namespaces and shared directories are set when a box is created. For those keys, stop and
   remove the box, then launch the application again: the launch creates the box from the kept
   profile, a pack box over its packs and a store box, the kind `kdos app install` makes, over its
   local image `kdos/<id>`, with an empty writable layer. `kdos app install` is needed only when
   that image is gone; it skips the images still built and creates the box over them.

   ```bash
   kdos-box stop app.hugin
   kdos-box remove app.hugin
   ```

Both kinds of box take the same keys. A store box is created by `distrobox create`, which passes
`memory`, `cpus` and `pids` to podman as `--memory`, `--cpus` and `--pids-limit`, and
`network=none` as `--network none`, a namespace with no interface; a `memory` or `cpus` value
made of anything but letters, digits and dots is refused with a warning and not applied. On
either kind, `memory` and `cpus` become
[cgroup]{.idx} limits only in a session started by autologin, which `kdos-getty` puts in your
delegated cgroup; in a session started any other way the engine accepts them and the kernel does
not apply them. See [Box profiles](../../kdos/04-programs/kdos-appbox.md#box-profiles) and
[memory and grants](../../kdos/04-programs/kdos-appbox.md#memory-and-grants).

### Granting a box more of the desktop

A box's clients reach the compositor through a tagged socket, and the compositor refuses them
screen capture, clipboard managers, lists of other windows, layer-shell surfaces, output power and
input methods unless the profile grants them by name:

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

These work on a pack box's writable layer, under `~/.local/share/kdos/boxes/<name>/`; the example
is a box imported from a set, which is named after its pack. A box built from a container image,
such as a store box, keeps its writes inside the engine's store, where they do not reach: on it,
`snapshot` refuses, and `clone` makes the new box on the same image without copying any work. A
pack box on the live medium keeps its writes on a temporary filesystem instead, and `snapshot`
refuses there too.

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

1. Create it. An `image:` base not yet in your image store is pulled from a registry over the
   network, with a line saying the content is unsigned; one already stored is used as it is:

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
   nothing for an image, such as Alpine's, that has none. `export` starts the box and reads the
   entry's `Exec`, `Name`, `Icon` and `StartupWMClass`, so an id that is not itself a program,
   such as `org.gnome.TextEditor`, runs the program its entry names; a word that names no entry
   is taken as the command itself. It writes
   `~/.local/share/applications/<desktop-id>.scratch.desktop`, whose command is
   `kdos-box run scratch` followed by the entry's command, and a shim
   `~/.local/bin/<command>@scratch`, named after the program in that command, which splits its
   own name at the last `@` and runs what `kdos-box run scratch <command>` runs, with its own
   arguments appended. `kdos-box unexport scratch <desktop-id>` removes both.

A box on a pack base (`base=pack:<id>`) needs no network, and its pack was verified when it was
installed. `base=box:<name>` makes a box on the other box's own base, read from its profile and
saved in the new one, with an empty writable layer; `kdos-box clone` copies the other box's work
as well.

### Stopping idle boxes

A box warmed at login keeps running until the machine shuts down, past Log Out and past the end of
the login, unless its profile sets `autostop`:

```bash
kdos-box profile app.hugin autostop=30m
kdos-box gc --dry-run                  # what the next sweep would stop
```

With nothing past its time, `gc --dry-run` prints `nothing to stop`.

The session runs `kdos-box gc` every ten minutes, from login until the login ends, in one loop per
login that `$XDG_RUNTIME_DIR/kdos/box-gc.pid` finds again on a session restart, so a box left
running after the login has ended waits for the next login's sweep. It stops a box that has run longer than its
`autostop` time, counted from when it started, and only after the compositor says the box has no
window open.

### Removing an application

```bash
kdos app remove app.hugin
```

It removes the box, the application's image, and each runtime image above the base that no
remaining box's chain names, then regenerates the launchers. The base image is never removed, and
neither is the box's profile. The box is removed with `kdos-box remove`, which refuses a running
box: the remove then prints `kdos-appbox: app.hugin was not uninstalled` and touches no image, so
stop the box first with `kdos-box stop app.hugin`.

### Updating an application

There is no update verb. `kdos app remove` and then `kdos app install` rebuild the application's
image and create its box again, as [Installing an application](#installing-an-application) says;
see [Updating](updating.md#updating-boxed-applications).

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
: The box create after the images failed, with podman's own message above this line; the images
  stay built. `kdos app install <id>` tries the box again without rebuilding them, and
  `kdos-box remove <id>` deletes the profile the failed create left. See
  [Installing an application](#installing-an-application).

A launch does nothing
: Run it from a terminal with the engine's messages shown: `kdos-appbox -v run <app>`. The
  stage timings of recent launches are in `$XDG_RUNTIME_DIR/kdos-appbox.trace`.

`could not start <name>`
: `kdos-box enter` prints this when the box will not start, without the engine's message, and
  `kdos-box start` prints nothing at all. Look at the container with
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

`could not create box '<name>'` on a launch
: The box does not exist and the launch could not make it. A pack box is made over its packs, and
  the line above names the cause, such as `kdos-packd is not running`. A store box is made over
  its local image `kdos/<id>`, with podman's message above; when that image is gone,
  `kdos app install <id>` builds it and creates the box again.

A profile change has no effect
: Namespace and directory keys apply at creation: stop and remove the box, then launch the
  application again, which creates either kind of box over its kept profile. A `memory` or `cpus` value that is not a
  plain size or number is refused at creation with a warning; see
  [Changing what a box may share](#changing-what-a-box-may-share). A `grant` change needs
  `kdos-comp -r`, and then reaches the next client, not a running one.

`kdos-box snapshot` says `<name> has nothing written to snapshot`
: The box was built from a container image, and its writes are in the engine's store, or it is a
  pack box on the live medium, whose writes are on a temporary filesystem under
  `/run/user/<uid>/kdos/boxes/`.

## Known gaps

- [A box can be granted only the eight named groups of Wayland globals](../../kdos/06-reference/known-gaps.md#a-box-can-be-granted-only-the-eight-named-groups-of-wayland-globals)
- [A boxed application is given your location without being asked](../../kdos/06-reference/known-gaps.md#a-boxed-application-is-given-your-location-without-being-asked)
- [A catalogue change reaches an installed application only when it is rebuilt](../../kdos/06-reference/known-gaps.md#a-catalogue-change-reaches-an-installed-application-only-when-it-is-rebuilt)
- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
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

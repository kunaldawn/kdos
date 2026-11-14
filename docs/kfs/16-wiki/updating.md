# Updating

A recipe pins its version, so what is newer for a KDOS machine arrives as a newer recipe tree.
`kdos update fetch` replaces `/ports` with the newest signed tree and fetches, beside it, the
release's signed [release index]{.idx}, `kdos-update.idx`, and through it alone the signed binhost
index and the signed slot image manifest. `kdos update check`
lists every installed package older than its recipe there and says what an update would make this
system. `kdos update apply` then writes the newer release into the inactive A/B slot, by one of
three lanes: the release's **slot image**, whole and checked file by file; the **binhost**'s
prebuilt packages; or, with `--source`, a **build** on this machine. Discover shows the same
update beside the applications' and installs it through `kdos-sysupdated`. The ISO carries only
the base, so the same commands also add software: `kdos update install` puts a port or a named
group of them into the running root, prebuilt from the release's binhost or built here. This page
covers checking, fetching and applying an update, installing software, taking a tree from a stick,
what to do after an update, the nightly check, and the vulnerability report. Boxed applications
are updated differently, at the end of [Usage](#usage).

## Installation

- [kdos-tools](#port-kdos-tools) installs the `kdos` command, whose `update`, `image`, `cve` and
  `restarts` subcommands are on this page. `kdos update` drives [kpkg](kpkg.md), which the
  bootstrap installs, and `kdos-bootctl`, from the same port, on a machine with two root slots.
- `kdos update` downloads with [`curl`]{.idx}, over HTTPS or from a local directory only, and
  unpacks with `tar`, `xz` and `zstd`, which the [curl](#port-curl), [tar](#port-tar),
  [xz](#port-xz) and [zstd](#port-zstd) ports install.
- [kdos-sysupdated](#port-kdos-sysupdated) serves `org.freedesktop.sysupdate1` and
  `io.kdos.Slots1` on the system bus; [Discover](#port-discover) with
  [kdos-discover](#port-kdos-discover)'s tray notifier shows and installs the system update
  through it, and the **System Versions** page of System Settings, from
  [kcm-kdos-system](#port-kcm-kdos-system), shows both slots and the update source.
- The base filesystem supplies the nightly check, [`/etc/kdos/timers.d/10-update-check.timer`]{.idx}, run
  by the service `timers` (see [Timers](timers.md)), the ring of keys a fetched tree is
  checked against, [`/etc/kdos/keys/tree/`]{.idx term="/etc/kdos/keys/tree"}, and the ring the
  release index, the binhost index and the slot image manifest are checked against,
  `/etc/kdos/keys/`.
- The image step [105_ports](../15-image/105-ports.md) stages the recipe tree that every image
  carries at `/ports`, with the [groups]{.idx} file `/ports/groups`, stamps it with the commit it
  was made from, and puts the release key's public half into the ring.
- The image step [017_stage](../15-image/017-stage.md) decides what the medium carries: the base
  of `ports/base`. Every other port is in the release's binhost, which
  [010_binhost](../15-image/010-binhost.md) writes.

The timer is enabled on the live medium and on an installed system alike.

## Configuration

### /etc/kdos/update.conf

Not shipped. The first four keys name places, each a URL or a directory such as a mounted USB
stick or an NFS share; the last two name how `kdos update install` installs. The `binhost` and
`image` places hold a `kdos-update.idx` with its `.sig`, and every file is fetched from where that
index says, never by its name:

| Key | Where `kdos update fetch` takes it from | With no key |
|---|---|---|
| `tree` | `kdos-ports.tar.xz` and its `.sig` | the latest KDOS release on GitHub |
| `binhost` | the release index through which the binhost index, `PACKAGES` and `PACKAGES.sig`, and the packages it names are found | the release `<version>` of a tree whose stamp names its release, whose index points into the releases `binhost-<version>-01`, `-02`, … |
| `image` | the release index through which the slot image manifest, `kdos-image.manifest` and its `.sig`, and its bundles are found | the latest KDOS release, whose index points into the release `image-<version>` |
| `carry` | a path, under `/etc` or `/var`, carried into a slot when an update arms it (see [A/B slots and rollback](ab-slots-and-rollback.md#updating-through-the-inactive-slot)); one line per path | nothing beyond the built-in list |
| `install` | `prebuilt` or `source`: the route `kdos update install` takes when no flag names one | `prebuilt` |
| `build-deps` | `prebuilt` or `source`: for a source install, whether each dependency the machine lacks is downloaded or built too | `prebuilt` |

Any other value of `install` or `build-deps` makes `kdos update install` exit 2. The System
Versions page's update-source setting writes `tree`, and its **Installing software** section
writes `install` and `build-deps`, both through kdos-sysupdated. A file
pointing every download at a stick:

```text
tree = /media/stick
binhost = /media/stick/binhost
image = /media/stick
```

`$KDOS_UPDATE_TREE`, `$KDOS_UPDATE_BINHOST` and `$KDOS_UPDATE_IMAGE` stand in for a missing key,
and `kdos update fetch --from` overrides the tree's. `sudo` clears the environment, so a variable
reaches the command only when it is set behind `sudo`:
`sudo env KDOS_UPDATE_TREE=/media/stick kdos update fetch`. See
[/etc/kdos/update.conf](../../kdos/06-reference/configuration.md#etckdosupdateconf).

### /etc/kdos/keys/tree/

A fetched tree is unpacked only when its signature verifies against a `*.pub` in this directory.
Its description, from `fs/etc/kdos/keys/tree/README`:

<!-- kfs:include fs/etc/kdos/keys/tree/README -->

The image step that fills it. An image built without the release key ships the ring empty, and
then refuses every fetched tree:

<!-- kfs:include script/phases/70_image/105_ports.sh lines=134-143 pin=68e72034c604 -->

### /etc/kdos/keys/

The release index, the binhost index and the slot image manifest are kept only when their signature
verifies against a `*.pub` directly in this directory; a release's index is signed with both the
release key and the binhost key. The image ships it with no key there, so until one is added all
of them are recorded as `unverified`: `kdos update apply` refuses, Discover offers no system update,
and `kdos update apply --source` is the one way to update, by building here. Its description, from
`fs/etc/kdos/keys/README`:

<!-- kfs:include fs/etc/kdos/keys/README -->

### /ports/TREE

The stamp naming the commit `/ports` was made from and when. `kdos update fetch` compares it with a
fetched tree's own stamp and refuses an older tree; a `/ports` with no stamp, from an image built
without a commit, accepts any signed tree. The same step writes it:

<!-- kfs:include script/phases/70_image/105_ports.sh lines=124-133 pin=91e34053e828 -->

On a booted image it reads, with the image's own commit and time:

```text
kdos-tree 1
commit <short commit>
time <seconds since 1970>
```

A tree a release published carries a fourth line, `version <release>`, which names the release
whose `kdos-update.idx` the binhost is found through.

### /etc/kpkg.conf

`kdos update` finds `/ports` through `PORT_REPO` in [`/etc/kpkg.conf`]{.idx}: the tree's root is the
directory above the first repository it names. See [kpkg](kpkg.md#etckpkgconf).

### The nightly check

<!-- kfs:include fs/etc/kdos/timers.d/10-update-check.timer -->

### State files

| File | Written by | Holds |
|---|---|---|
| [`/var/lib/kdos/update.state`]{.idx} | `fetch`, `apply` | The outcome of the last fetch of the tree, the release index with the binhost index, and the release index with the image manifest, and of the last apply: `ok`, `absent`, `unverified`, `no-key`, `no-network`, `fetch-failed`, `build-failed`, `acquired` or `cancelled`, with a time and a sentence |
| [`/var/lib/kdos/update.state.lock`]{.idx} | `fetch`, `apply` | The lock both hold, so a tree is never replaced under a build |
| [`/var/lib/kdos/update.state.sig`]{.idx} | `fetch` | The signature of the tree last installed |
| [`/var/lib/kdos/update.json`]{.idx} | The nightly check, `fetch`, `apply`, kdos-sysupdated at start | What `check --json` found: what is behind, what is offered and by which lane; kdos-sysupdated serves Discover from it |
| `/var/lib/kdos/binhost/` | `fetch`, `apply` | The verified binhost index, the release index it was fetched by (`INDEX`, `INDEX.sig`, `ORIGIN`), and the packages it names, kept until a newer index names others |
| `/var/lib/kdos/image/offer/` | `fetch` | The verified manifest of the newest slot image and the release index that names its bundles |
| `/var/lib/kdos/image/MANIFEST` | `apply` by the image lane | The manifest of the image the root was written from, which the next image lane reads to pick its delta |
| [`/var/lib/kdos/world`]{.idx} | `install` | The world set: each port installed on purpose, one name per line, which every update keeps installed |
| [`/var/lib/kdos/origin`]{.idx} | `install`, `apply` | One `<package> prebuilt\|source` line for each package an install or an update added; a package with no line came prebuilt, as everything the image brought did. Carried into a slot beside the world set |
| [`/var/lib/kdos/groups-pending`]{.idx} | the installer | The groups chosen at install time that could not be installed then, one id per line, for `install --pending` |

A freshly booted image has none of them. The first `fetch` or `apply` writes `update.state` and its
lock, and `update.json` appears at the nightly check's first run, or sooner when a `fetch` or an
`apply` is run as root. `update.state.sig` appears only once a fetch has installed a tree, or has
found the published tree to be the one already installed.

The full list is in [Filesystem and IPC](../../kdos/06-reference/filesystem-and-ipc.md).

## Usage

`ports/publish --release` publishes a release's tree, binhost and slot image with the index that
finds them, but no published release yet carries them, so no update has been taken from one; see
[Known gaps](#known-gaps). The update lanes are exercised against fixtures by
`testing/selftest.sh`.

### From the desktop

Discover's tray icon shows when an update is offered. Open Discover's **Updates** page: the KDOS
system update is listed with its version beside any application updates, and its text names the
lane it comes by and the size still to download. **Update All** writes the inactive slot, and **Restart to update** reboots
into it. The update is confirmed a minute into the next session, or rolled back by itself when
that boot does not get there (see [A/B slots and rollback](ab-slots-and-rollback.md)). System
Settings → **System Versions** shows what each slot holds and why the last update stopped, if it
did.

### Checking what is behind

1. Ask:

   ```bash
   kdos update check
   ```

   `kdos update` with no verb does the same. It lists each installed package whose version, or
   release, is older than its recipe in `/ports`, and which of them the verified binhost index
   covers; counts the installed packages that have no recipe there; names the tree's commit; says
   whether a verified slot image of a newer release is cached; and reports the last problem a fetch
   or an apply recorded.

2. Read the exit status: 0 nothing is behind, 1 something is behind, 2 there is no tree or no
   package database. A recorded problem does not change it: the check above exits 0.

3. For a script, ask for JSON, or write it to a file:

   ```bash
   kdos update check --json
   kdos update check --out ~/update.json
   ```

   The document's `offer` is the version an update would make this system and `lane` is where it
   comes from: `image`, or `binhost` when every package behind is covered by a verified index or
   was built here. `builds` counts the packages behind that the update builds here, because their
   origin is `source`; they count as covered. With neither lane, nothing is offered to Discover,
   and `apply --source` is the way.

### Fetching a newer tree

1. Fetch the newest published tree. It is downloaded with its signature, the signature is checked
   against `/etc/kdos/keys/tree/`, and only then is the tree unpacked and exchanged with `/ports`.
   The release index follows, checked against `/etc/kdos/keys/`, and through it the binhost index
   and the slot image manifest, each kept only with the size and hash the index gives and when its
   own signature verifies; a release that publishes no image is recorded as `absent`, which is not
   a failure, and an index for another release than the tree is `unverified`:

   ```bash
   sudo kdos update fetch
   ```

2. Read the result: exit 0 installed or already current, 1 refused or failed, 2 not attempted (not
   root, another update running, or `/ports` inside a git checkout). Run as an ordinary user it
   prints `kdos update fetch replaces the ports tree and needs root` and exits 2.

3. Check against the new tree, or do both in one command:

   ```bash
   kdos update check
   sudo kdos update check --fetch
   ```

A tree whose stamp is older than the installed one is refused, and a tree with the same commit is
reported as current.

### Fetching from a stick

A stick, or any directory, holding `kdos-ports.tar.xz` and `kdos-ports.tar.xz.sig` from a release
can stand in for the network for the tree, and a binhost or slot image directory as a build wrote
it (`build/binhost`, or what `script/release-image.sh` wrote), each with its own
`kdos-update.idx`, for theirs (see [/etc/kdos/update.conf](#etckdosupdateconf)):

1. Mount it, for example at `/media/stick`.
2. Fetch from it:

   ```bash
   sudo kdos update fetch --from /media/stick
   ```

   An image with no tree key stops at `no-key` before reading the stick. On one with a key, the
   signature is fetched first: a directory without the two files fails with curl's
   `(37) Could not open file /media/stick/kdos-ports.tar.xz.sig`, then
   `kdos update: file:///media/stick/kdos-ports.tar.xz could not be downloaded`, exit 1, and
   `fetch-failed` in `/var/lib/kdos/update.state`.

3. To make the stick the usual source, write `tree = /media/stick` into `/etc/kdos/update.conf`.

The sources a `--source` update builds from still come from the network, from the KDOS source
archive and then upstream. `KDOS_SOURCES_BASE`, set to an absolute path, names a directory laid out as the
archive is, `<tag>/<asset>`, in its place, and like any variable it has to be set behind `sudo`:
`sudo env KDOS_SOURCES_BASE=/media/archive kdos update apply`. See
[Fetching sources on a machine](../../kdos/03-architecture/packaging.md#fetching-sources-on-a-machine).

### Applying an update

1. See what would be done. This needs no root:

   ```bash
   kdos update apply --dry-run
   ```

   With nothing behind it prints `nothing to take: everything installed matches the ports tree`.

2. Apply:

   ```bash
   sudo kdos update apply --mount
   ```

   On a machine with two root slots `apply` writes the inactive one, which `--mount` mounts at
   `/run/kdos/slot-<x>` for the run; without `--mount` the slot has to be mounted already, wherever
   it is, and `apply` finds it by its UUID in the boot state. It takes the first lane that can
   deliver the update:

   - **The slot image**, when a verified manifest of a newer release is cached. The slot is
     synchronised, not formatted: files the slot or the running root already hold are copied and
     checked, the delta from the version this root was written from is streamed, and the full image
     fills only what is still missing. Every file is checked against the signed manifest, so a
     damaged delta falls back to the full image. A slot image is the base alone, so `apply` then
     installs the world set, and whatever it lacks, into the slot: prebuilt from the same
     release's binhost, or built here for a package whose origin is `source`. It decides that
     before writing anything: when a prebuilt package of the world set, or a dependency of one, is
     not in that binhost, the lane refuses with `needs-source` and writes nothing. With no world
     set there is no such step.
   - **The binhost**, when the verified index covers every package behind that came prebuilt.
     kpkg installs each prebuilt package after its three equality tests (architecture, build
     configuration, recipe); one package that fails them refuses the whole lane before the slot is
     written, with `N packages need a source build`. A package whose line in
     `/var/lib/kdos/origin` says `source` was built here and is built here again, in a chroot of
     the slot, so it needs no index.
   - **`--source`**, a build on this machine: every source of every package behind is downloaded
     before anything compiles, then each package is built and installed with `kpkg install -f`,
     dependencies first, in a chroot of the inactive slot against that slot's libraries.

   `--image`, `--binhost` and `--source` name a lane instead.

3. On a machine with two slots, `apply` then runs `kpkg check --all` on the slot, carries this
   machine's configuration into it, copies `/ports` into it unless an image brought its own,
   records its version, puts its kernel on the EFI system partition with `kdos-bootctl deploy`,
   and marks it to be tried with `kdos-bootctl try`. A failure at any step leaves the slot untried.
   Reboot to try it.

4. On a machine with one root, `apply` prints `no A/B slots — updating the running root in place`
   and updates the running root. On a machine with two slots, `--in-place` updates the running
   root instead of the slot, and gives up the rollback.

`apply --acquire` does only the long half, the download, the write and the check, and records the
slot as `acquired`; `apply --arm` does the rest. Discover runs the two as its download and its
install step. While the running boot is itself a trial, `apply` refuses: the other slot is the
confirmed way back until this one is confirmed or rolled back.

`--root DIR` installs into `DIR` instead of choosing a root. It does nothing about slots, even when
`DIR` is the inactive slot: no `kpkg check`, no carry, no copy of `/ports`, no
`kdos-bootctl deploy` and no `try`, so a slot updated this way is never booted as a candidate.

### Installing software

The base the ISO carries is Plasma, the core KDE applications, Firefox, the toolchain and the
system's own tools; everything else the release built is in its binhost. A port reaches this
machine by one of three routes:

| Route | How | What it takes |
|---|---|---|
| Prebuilt | `kdos update install --prebuilt <port>`, or Discover's **KDOS system** origin | The port and every dependency it lacks from the release's binhost, each checked against the signed index |
| Built here | `kdos update install --source <port>`, or Discover's **KDOS, built on this computer** origin | Every source downloaded, then the port built and installed on this machine |
| Boxed | `kdos app install <id>`, or Discover's boxed applications | A container image built on this machine from the catalogue; see [Boxes](boxes.md) |

`/etc/kdos/update.conf`'s `install =` names the route `kdos update install` takes with no flag,
and `build-deps =` whether a source install downloads or builds what the port lacks (see
[/etc/kdos/update.conf](#etckdosupdateconf)); System Settings → **System Versions** →
**Installing software** writes both. Discover does not read `install =`: it lists one
application with both origins in its source picker, and asks for the administrator password every
time it builds.

1. Have the release's binhost index. A machine that has fetched an update has it; on one that
   has not, take only the index of the tree in place, not a newer tree:

   ```bash
   sudo kdos update fetch --binhost-only
   ```

2. See what an install would do. This needs no root:

   ```bash
   kdos update install --dry-run <port>
   ```

   One line per package, `<pkg> prebuilt <file> <bytes>` or `<pkg> source <version>-<release>`,
   then `total <prebuilt-count> <bytes> <source-count>`.

3. Install:

   ```bash
   sudo kdos update install <port>
   sudo kdos update install --source <port>
   sudo kdos update install --source --build-deps=source <port>
   ```

   Each package prints `==> installing <pkg> (prebuilt)` or `==> installing <pkg> (built here)`.
   A source install runs `kpkg fetch` for every source first, then builds under
   `/etc/kdos/chroot.env`, so what it builds has the build configuration the binhost's packages
   have; under `build-deps = prebuilt` a dependency the binhost does not hold is built too. The
   port joins `/var/lib/kdos/world`, and each package it added gets its route in
   `/var/lib/kdos/origin`, which later updates keep: a package built here is built here again.

An install never rebuilds a package that is installed. Asking for an installed port by the other
route installs that port again, alone, by the route asked for.

A source install readies the machine first, because the image carries each package without its
headers and static libraries ([Development files](kpkg.md#development-files)) and carries no
build tools. It installs each member of the `build-tools` group the machine lacks prebuilt, `==> installing <pkg>
(prebuilt, for building)` per package; then it installs again whole, from the binhost, every
installed package the build compiles against that lacks its development files, `==> restoring the
development files of <pkg> (prebuilt)`. The dry run lists both first, as `<pkg> prebuilt …` and
`<pkg> dev <file> <bytes>` lines. To bring a package's development files back without building
anything:

```bash
sudo kdos update install --dev zlib
sudo kdos update install --dev kde-devtools
```

#### Groups

`/ports/groups` names sets of ports a machine installs by one name: the KDE Gear sets
(`kde-games`, `kde-education`, `kde-pim`, `kde-multimedia`, `kde-graphics`, `kde-utilities`,
`kde-devtools`), `office`, `graphics`, `internet`, `multimedia`, `games`, `science`,
`development` and `windows`. List them, with what each would add:

```bash
kdos update groups
kdos update groups --json
```

Each line names the group, its title, and either `installed` or how many of its members are
missing, how many packages that is, and the download size from the cached index (`not all
prebuilt` when the binhost does not hold every one; no size at all without an index). Install one
as you would a port:

```bash
sudo kdos update install office
```

Each member is installed as the port it is and joins the world set by its own name, so a member a
later tree drops from the group stays installed.

#### At install time and on the live medium

The installer's **Applications** page lists the groups as **KDOS applications**, none chosen by
default, above the boxed applications; an answer file names them with `groups =`, separately from
the boxed groups of `apps =` (see [kinstall](../../kdos/04-programs/kinstall.md#the-applications-step)).
Its **Packs** step runs `kdos update fetch --binhost-only` with the cache on the target and then
`kdos update install --prebuilt --root <target> <id>` for each group. When the fetch or a group
fails, for instance with no network, the ids go to `/var/lib/kdos/groups-pending` on the target
with a warning, and the install still succeeds. Install them once the machine is online:

```bash
sudo kdos update install --pending
```

Each group that installs leaves the file; with nothing recorded it prints `nothing is pending`.

On the live medium, `kdos update install` works the same way, into the overlay in RAM: what it
installs is gone at the next restart, unless the medium has a persistence store (see
[Live persistence](live-persistence.md)).

`--root DIR` installs into a root that is not running, `DIR` absolute, by the prebuilt route only:
a build compiles against the root it runs in, so `--source` with `--root` exits 2.

### After an update

1. List the running processes that still use code the update replaced; each keeps the old code
   until it restarts. Run it as root to include the services: an ordinary user cannot read another
   account's processes, and `kdos restarts` skips them, so without root its answer covers only
   your own:

   ```bash
   sudo kdos restarts
   ```

   With nothing replaced it prints `Nothing is running deleted code.` and exits 0; it exits 1 when
   something needs restarting.

2. Restart those services (`sudo service restart <name>`; see [Services](services.md)), or log out
   and back in for the desktop's own programs. A new kernel runs from the next boot.

3. After a slot switch, list the configuration files where the new version's default and this
   machine's copy differ, and merge what you want from each:

   ```bash
   find /etc -name '*.kdos-new'
   ```

4. Read what the running version changed: `kdos update notes`.

5. If the update changed the artwork, re-apply your accent to your home:

   ```bash
   kdos update theme
   ```

### Checking for known vulnerabilities

```bash
kdos cve
kdos cve openssl
kdos doctor --cve
```

`kdos cve` compares the installed versions against a copy of Alpine's security database shipped at
`/usr/share/kdos/secdb.txt`, with no network. It exits 1 when any package is behind a recorded fix.
A package the database does not carry is counted as unknown, not as clean, and the report gives the
database's date and warns when it is more than 180 days old. The report opens with the database's
date and age (`Alpine secdb of 2026-08-12 (70 days old)` on one image), lists each package behind a
fix with the version that fixed it and its CVE numbers, then the Rust programs that bundle a copy
of a library under its own name, and ends with the counts: on the full image, 2104 checked, 1803
not in the database. `kdos doctor --cve` prints the same report. The database arrives with
[kdos-tools](#port-kdos-tools), so a newer one comes with an update of that port. See
[Vulnerability tracking](../../kdos/03-architecture/packaging.md#vulnerability-tracking).

### Updating boxed applications

A boxed application is not part of `kdos update`. It is a stack of container images this machine
built, and an image that exists is never rebuilt, so the update is a removal and a fresh install:

```bash
kdos app remove app.hugin
kdos app install app.hugin
```

The removal takes the application's own image and every runtime image no other box needs; the base
image is kept, so the Debian userland under every application is not rebuilt. Installing an
application that is already installed builds nothing and reports a failure. The fresh install
builds the images and then creates the box on them; see
[Installing an application](boxes.md#installing-an-application) and
[Updating](../../kdos/02-user-guide/applications.md#updating) in the user guide.

### On the live medium

`/ports` on the live medium lies in the overlay's read-only lower layer, which cannot be renamed, so
`fetch` removes the old entries and moves the new ones in one at a time. Everything `fetch` and
`apply` write goes into the overlay in RAM and is gone at power-off unless the medium has a
persistence store ([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)). The
live medium has no slot state, so `apply` updates the running root. The software the base leaves
out is installed there as on an installed machine, into the same overlay; see
[Installing software](#installing-software).

## Tips

- `kdos update check` answers against whatever tree, index and manifest are cached, so it works
  offline; only `--fetch` reaches the network.
- An image update downloads only the delta from the release this root was written from. A
  `--source` update compiles on the machine, and a large set takes hours.
- A machine that will never take a published update can keep `--source`, which needs no key in
  `/etc/kdos/keys/`.
- `/ports` is replaced whole by every fetch. Keep a recipe you changed outside it, as
  [Local rebuilds and kdos march](local-rebuilds-and-march.md) shows.

## Troubleshooting

Discover offers no system update, or `kdos update check` reports a problem
: `check` names the first of `no-tree`, then the last fetch's `no-key`, `no-network` or
  `fetch-failed`, then
  the last apply's `no-network`, `fetch-failed` or `build-failed`. The sentence beside it is in
  `/var/lib/kdos/update.state`, which also has the `binhost` and `image` lines:

  ```bash
  cat /var/lib/kdos/update.state
  ```

`kdos update check` exits 2
: There is no tree at the root `PORT_REPO` names, or no package database. Check `/ports/core` and
  `/etc/kpkg.conf`.

`no-key` on every fetch
: The image's ring, `/etc/kdos/keys/tree/`, holds no key: the image was built without the
  release key, so it can verify no published tree, and `fetch` stops before downloading it,
  exit 1. With no key in `/etc/kdos/keys/` either, the `binhost` and `image` lines say `no-key`
  too, and nothing at all is downloaded. `ls /etc/kdos/keys/tree/` shows whether a `kdos-release.pub` is there; on such an image
  the directory holds only its `README`. `kdos update apply` still builds whatever the recipes in
  `/ports` are ahead of. See [Known gaps](#known-gaps).

`fetch-failed` on every fetch
: A release that carries no `kdos-ports.tar.xz` fails at the download: the latest release is such
  a one, and `sudo kdos update fetch` with no `update.conf` prints
  `curl: (22) The requested URL returned error: 404` and exits 1. See
  [Known gaps](#known-gaps).

`fetch-failed` after the tree downloaded
: The sentence in `/var/lib/kdos/update.state` names the cause, such as a signature that does not
  verify against `/etc/kdos/keys/tree/`, a published tree older than the one installed, or an
  archive that is not a ports tree. In each of these `/ports` keeps the tree it had.

`fetch-failed` or `no-network` after `apply`
: `apply` fetches every source before building anything, and stopped there: it prints
  `nothing was built`, and `update.state` lists the packages whose sources neither the archive nor
  upstream had, or says the network could not be reached. Fetch one by hand to see why:
  `sudo kpkg fetch <port>`.

`unverified` on the `binhost` or `image` line
: No key in `/etc/kdos/keys/` verifies the release index, the binhost index or the manifest, a file
  is not the one the release index names, or the index is for another release; nothing was kept,
  and Discover offers nothing. Add the key that signed the release, or update with `--source`.

`N packages need a source build`
: The binhost does not cover every package behind (another architecture, another build
  configuration, or a recipe the index predates). The binhost lane refuses whole; use `--source`.
  From `kdos update install`, the same line ends with `` `kdos update install --source <port>`
  builds it here ``: the prebuilt route refuses the whole install when the index does not hold
  every package it needs.

`<pkg> is older than its recipe; apply the system update first`
: `kdos update install` refuses when a dependency the port needs is installed and behind its
  recipe in `/ports`, because the new port was built against the newer one. Apply the update, then
  install.

`needs-source` from the image lane
: `N package(s) of what was installed on purpose is not in release X's binhost (...)`: a port in
  the world set, or a dependency of one, came prebuilt and the new release's binhost does not hold
  it, so the slot image would boot without it. Nothing was written. Install each package the
  message names with `kdos update install --source`, so its origin is `source` and every update
  builds it here, or update with `--source`.

`fetch` exits 2
: It needs root (`kdos update fetch replaces the ports tree and needs root`), another `fetch` or
  `apply` holds the lock, or `/ports` is inside a git checkout, which belongs to git and is never
  replaced.

`no-network`
: `curl` could not reach the server. Check the connection with `nmcli general status`, or fetch
  from a stick with `--from`.

`build-failed`
: A package did not build, or, on a machine with two slots, the slot failed its `kpkg check`, the
  copy of `/ports`, the kernel deploy or the `try`; the sentence in `update.state` says which. A
  slot that failed any of these is not marked to be tried. A package's output is in the terminal
  `apply` ran in; rebuild that port alone with `sudo env KPKG_FETCH=1 kpkg install -f <port>` to see
  it again (see [kpkg](kpkg.md)).

`apply` refuses: the inactive slot is not mounted
: Pass `--mount`, mount it and run `apply` again, or pass `--in-place`. See
  [A/B slots and rollback](ab-slots-and-rollback.md).

## Known gaps

- [The update path has not been run against a published tree](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)
- [An update with --root compiles against the running system](../../kdos/06-reference/known-gaps.md#an-update-with---root-compiles-against-the-running-system)
- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only)
- [A catalogue change reaches an installed application only when it is rebuilt](../../kdos/06-reference/known-gaps.md#a-catalogue-change-reaches-an-installed-application-only-when-it-is-rebuilt)

## See also

- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update): every verb, option and exit status
- [Install routes and groups](../../kdos/04-programs/kdos-command.md#install-routes-and-groups): `install`, `groups` and the origin file
- [The base image](../../kdos/03-architecture/packaging.md#the-base-image): what the ISO carries and why a base ISO and its binhost are one unit
- [Updating a machine](../../kdos/03-architecture/packaging.md#updating-a-machine): how a tree, `kpkg` and the slots fit together
- [Keeping it current](../../kdos/02-user-guide/administration.md#keeping-it-current): the user guide's own section
- [A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection): how a tried slot is confirmed or rolled back
- [Discover](../../kdos/04-programs/discover.md): the system update, the KDOS backend and its two origins
- [kinstall](../../kdos/04-programs/kinstall.md#the-applications-step): the groups an install adds
- [kpkg](kpkg.md), [A/B slots and rollback](ab-slots-and-rollback.md), [Timers](timers.md), [Boxes](boxes.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 431.* Previous: [430. kpkg](kpkg.md) · [Contents](../README.md) · Next: [432. Local rebuilds and kdos march](local-rebuilds-and-march.md)

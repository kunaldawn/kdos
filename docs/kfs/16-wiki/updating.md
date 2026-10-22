# Updating

A KDOS machine is updated by building it from source. A recipe pins its version, so what is newer
arrives as a newer recipe tree: `kdos update fetch` replaces `/ports` with the newest signed tree,
`kdos update check` lists every installed package older than the recipe there, and `kdos update apply`
fetches the sources of those packages and builds and installs each one on the machine. There is no
binary package server. This page covers checking, fetching and applying an update, taking a tree
from a stick, what to do after an update, the nightly check that feeds the panel's badge, and the
vulnerability report. Boxed applications are updated differently, at the end of [Usage](#usage).

## Installation

- [kdos-tools](#port-kdos-tools) installs the `kdos` command, whose `update`, `cve` and
  `restarts` subcommands are on this page. `kdos update` drives [kpkg](kpkg.md), which the
  bootstrap installs, and `kdos-bootctl`, from the same port, on a machine with two root slots.
- `kdos update fetch` downloads with [`curl`]{.idx} and unpacks with `tar` and `xz`, which the
  [curl](#port-curl), [tar](#port-tar) and [xz](#port-xz) ports install.
- The base filesystem supplies the nightly check, [`/etc/kdos/timers.d/10-update-check.timer`]{.idx}, run
  by the service `timers` (see [Timers](timers.md)), and the ring of keys a fetched tree is
  checked against, [`/etc/kdos/keys/tree/`]{.idx term="/etc/kdos/keys/tree"}.
- The image step [105_ports](../15-image/105-ports.md) stages the recipe tree that every image
  carries at `/ports`, stamps it with the commit it was made from, and puts the release key's
  public half into the ring.

The timer is enabled on the live medium and on an installed system alike.

## Configuration

### /etc/kdos/update.conf

Not shipped. It has one key, `tree`, which says where `kdos update fetch` takes
`kdos-ports.tar.xz` and `kdos-ports.tar.xz.sig` from: a URL, or a directory such as a mounted USB
stick or an NFS share. With no file, the tree comes from the latest KDOS release on GitHub. A file
pointing at a stick is one line:

```text
tree = /media/stick
```

`$KDOS_UPDATE_TREE` overrides the file, and `kdos update fetch --from` overrides both. See
[/etc/kdos/update.conf](../../kdos/06-reference/configuration.md#etckdosupdateconf).

### /etc/kdos/keys/tree/

A fetched tree is unpacked only when its signature verifies against a `*.pub` in this directory.
Its description, from `fs/etc/kdos/keys/tree/README`:

<!-- kfs:include fs/etc/kdos/keys/tree/README -->

The image step that fills it. An image built without the release key ships the ring empty, and
then refuses every fetched tree:

<!-- kfs:include script/phases/70_image/105_ports.sh lines=132-141 pin=68e72034c604 -->

### /ports/TREE

The stamp naming the commit `/ports` was made from and when. `kdos update fetch` compares it with a
fetched tree's own stamp and refuses an older tree; a `/ports` with no stamp, from an image built
without a commit, accepts any signed tree. The same step writes it:

<!-- kfs:include script/phases/70_image/105_ports.sh lines=122-131 pin=91e34053e828 -->

On a booted image it reads, with the image's own commit and time:

```text
kdos-tree 1
commit <short commit>
time <seconds since 1970>
```

### /etc/kpkg.conf

`kdos update` finds `/ports` through `PORT_REPO` in [`/etc/kpkg.conf`]{.idx}: the tree's root is the
directory above the first repository it names. See [kpkg](kpkg.md#etckpkgconf).

### The nightly check

<!-- kfs:include fs/etc/kdos/timers.d/10-update-check.timer -->

### State files

| File | Written by | Holds |
|---|---|---|
| [`/var/lib/kdos/update.state`]{.idx} | `fetch`, `apply` | The outcome of the last fetch and the last apply: `ok`, `no-network`, `fetch-failed` or `build-failed`, with a time and a sentence |
| [`/var/lib/kdos/update.state.lock`]{.idx} | `fetch`, `apply` | The lock both hold, so a tree is never replaced under a build |
| [`/var/lib/kdos/update.state.sig`]{.idx} | `fetch` | The signature of the tree last installed |
| [`/var/lib/kdos/update.json`]{.idx} | The nightly check, `fetch`, `apply` | What `check --json` found; the panel's update badge reads it |

A freshly booted image has none of them: the first `fetch` writes `update.state` and its lock, and
`update.json` appears at the nightly check's first run.

The full list is in [Filesystem and IPC](../../kdos/06-reference/filesystem-and-ipc.md).

## Usage

On a booted live medium `check`, `apply --dry-run`, `restarts`, `cve` and each way `fetch` refuses
have been run, and the outputs below are theirs. No `fetch` has installed a tree and no `apply` has
built anything: no release yet carries `kdos-ports.tar.xz`, and the image's tree ring is empty, so
there has been no newer signed tree to take. See [Known gaps](#known-gaps).

### Checking what is behind

1. Ask:

   ```bash
   kdos update check
   ```

   `kdos update` with no verb does the same. It lists each installed package whose version, or
   release, is older than its recipe in `/ports`, counts the installed packages that have no recipe
   there, names the tree's commit, and reports the last problem a fetch or an apply recorded. On
   an image whose tree matches what it carries, after a fetch that failed:

   ```text
   KDOS update  —  the ports tree against what is installed

     0 behind, 0 with no recipe on this machine
     tree <commit>, <date> at /ports
     fetch-failed: https://github.com/kunaldawn/kdos/releases/latest/download/kdos-ports.tar.xz could not be downloaded
   ```

2. Read the exit status: 0 nothing is behind, 1 something is behind, 2 there is no tree or no
   package database. A recorded problem does not change it: the check above exits 0.

3. For a script, ask for JSON, or write it to a file:

   ```bash
   kdos update check --json
   kdos update check --out ~/update.json
   ```

   ```text
   {"behind":0,"orphans":0,"tree":"<commit>","problem":null,"detail":"","packages":[]}
   ```

### Fetching a newer tree

1. Fetch the newest published tree. It is downloaded with its signature, the signature is checked
   against `/etc/kdos/keys/tree/`, and only then is the tree unpacked and exchanged with `/ports`:

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
can stand in for the network for the tree:

1. Mount it, for example at `/media/stick`.
2. Fetch from it:

   ```bash
   sudo kdos update fetch --from /media/stick
   ```

   The signature is fetched first: a directory without the two files fails with curl's
   `(37) Could not open file /media/stick/kdos-ports.tar.xz.sig`, then
   `kdos update: file:///media/stick/kdos-ports.tar.xz could not be downloaded`, exit 1, and
   `fetch-failed` in `/var/lib/kdos/update.state`.

3. To make the stick the usual source, write `tree = /media/stick` into `/etc/kdos/update.conf`.

The sources an update builds from still come from the network, from the KDOS source archive and
then upstream. `KDOS_SOURCES_BASE` names a directory laid out as the archive is,
`<tag>/<asset>`, in its place; see
[Fetching sources on a machine](../../kdos/03-architecture/packaging.md#fetching-sources-on-a-machine).

### Applying an update

1. See what would be built. This needs no root:

   ```bash
   kdos update apply --dry-run
   ```

   On the live medium, with nothing behind:

   ```text
   no A/B slots — updating the running root in place
   nothing to take: everything installed matches the ports tree
   ```

2. On a machine with two root slots, mount the inactive slot first; `apply` finds it by its UUID
   wherever it is mounted, and refuses when it is not mounted. See
   [A/B slots and rollback](ab-slots-and-rollback.md).

3. Apply:

   ```bash
   sudo kdos update apply
   ```

   It downloads every source of every package that is behind before it compiles anything, so a
   machine that is offline, or a source neither the archive nor upstream has, stops the run in
   minutes. It then builds and installs each package with `kpkg install -f`, with `KPKG_FETCH=1`
   so that a dependency the new tree adds fetches its own sources.

4. On a machine with two slots, `apply` then runs `kpkg check --all` on the slot it installed into,
   copies `/ports` into it, puts its kernel on the EFI system partition with `kdos-bootctl deploy`,
   and marks it to be tried with `kdos-bootctl try`. A failed package, check, copy or deploy leaves
   the slot untried. Reboot to try it.

5. On a machine with one root, `apply` prints `no A/B slots — updating the running root in place`
   and updates the running root. On a machine with two slots whose inactive slot is not mounted,
   `--in-place` updates the running root instead of refusing, and gives up the rollback.

`--root DIR` installs into `DIR` instead of choosing a root. A machine installed by `kinstall` has
slot A only, so until slot B is made and recorded with `kdos-bootctl set-slot`, it updates in
place.

### After an update

1. List the running processes that still use code the update replaced; each keeps the old code
   until it restarts:

   ```bash
   kdos restarts
   ```

   With nothing replaced it prints `Nothing is running deleted code.`

2. Restart those services (`sudo service restart <name>`; see [Services](services.md)), or log out
   and back in for the desktop's own programs. A new kernel runs from the next boot.

3. If the update changed the artwork, regenerate your theme files:

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
application that is already installed builds nothing and reports a failure. As shipped, the
install stops after the images, at creating the box; see
[Installing an application](boxes.md#installing-an-application) and
[Updating](../../kdos/02-user-guide/applications.md#updating) in the user guide.

### On the live medium

`/ports` on the live medium lies in the overlay's read-only lower layer, which cannot be renamed, so
`fetch` removes the old entries and moves the new ones in one at a time. Everything `fetch` and
`apply` write goes into the overlay in RAM and is gone at power-off unless the medium has a
persistence store ([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)). The
live medium has no slot state, so `apply` updates the running root.

## Tips

- The panel's update badge counts the packages behind. It turns to the warning colour, or shows `!`
  when nothing is behind, when the last fetch or apply failed or the machine has no tree; hovering
  it says which.
- `kdos update check` answers against whatever tree is in `/ports`, so it works offline; only
  `--fetch` reaches the network.
- An update compiles on the machine, and a large set takes hours.
- `/ports` is replaced whole by every fetch. Keep a recipe you changed outside it, as
  [Local rebuilds and kdos march](local-rebuilds-and-march.md) shows.

## Troubleshooting

The badge shows `!`, or `kdos update check` reports a problem
: `check` names the first of `no-tree`, then the last fetch's `no-network` or `fetch-failed`, then
  the last apply's `no-network`, `fetch-failed` or `build-failed`. The sentence beside it is in
  `/var/lib/kdos/update.state`:

  ```bash
  cat /var/lib/kdos/update.state
  ```

`kdos update check` exits 2
: There is no tree at the root `PORT_REPO` names, or no package database. Check `/ports/core` and
  `/etc/kpkg.conf`.

`fetch-failed` on every fetch
: The image's ring, `/etc/kdos/keys/tree/`, holds no key when the image was built without the
  release key, and then every tree is refused. `ls /etc/kdos/keys/tree/` shows whether a
  `kdos-release.pub` is there; on an image built without it the directory holds only its
  `README`. A release that carries no `kdos-ports.tar.xz` fails before that, at the download:
  the latest release is such a one, and `sudo kdos update fetch` with no `update.conf` prints
  `curl: (22) The requested URL returned error: 404` and exits 1. See
  [Known gaps](#known-gaps).

`fetch` exits 2
: It needs root (`kdos update fetch replaces the ports tree and needs root`), another `fetch` or
  `apply` holds the lock, or `/ports` is inside a git checkout, which belongs to git and is never
  replaced.

`no-network`
: `curl` could not reach the server. Check the connection with `nmcli general status`, or fetch
  from a stick with `--from`.

`build-failed`
: One package did not build. Its output is in the terminal `apply` ran in; rebuild that port alone
  with `sudo env KPKG_FETCH=1 kpkg install -f <port>` to see it again (see [kpkg](kpkg.md)).

`apply` refuses: the inactive slot is not mounted
: Mount it and run `apply` again, or pass `--in-place`. See
  [A/B slots and rollback](ab-slots-and-rollback.md).

The time zone is UTC after an update
: Set it again with `kdos-power timezone <Area/City>`; see
  [Known gaps](#known-gaps).

## Known gaps

- [The update path has not been run against a published tree](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)
- [Nothing creates or first fills the second root slot](../../kdos/06-reference/known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot)
- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only)
- [A tzdata upgrade can reset the time zone to UTC, once](../../kdos/06-reference/known-gaps.md#a-tzdata-upgrade-can-reset-the-time-zone-to-utc-once)
- [There is no public binary host](../../kdos/06-reference/known-gaps.md#there-is-no-public-binary-host)
- [A catalogue change reaches an installed application only when it is rebuilt](../../kdos/06-reference/known-gaps.md#a-catalogue-change-reaches-an-installed-application-only-when-it-is-rebuilt)

## See also

- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update): every verb, option and exit status
- [Updating a machine](../../kdos/03-architecture/packaging.md#updating-a-machine): how a tree, `kpkg` and the slots fit together
- [Keeping it current](../../kdos/02-user-guide/administration.md#keeping-it-current): the user guide's own section
- [A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection): how a tried slot is confirmed or rolled back
- [kpkg](kpkg.md), [A/B slots and rollback](ab-slots-and-rollback.md), [Timers](timers.md), [Boxes](boxes.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 405.* Previous: [404. kpkg](kpkg.md) · [Contents](../README.md) · Next: [406. Local rebuilds and kdos march](local-rebuilds-and-march.md)

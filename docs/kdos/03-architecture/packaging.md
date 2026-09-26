# Packaging

This page explains how software on the KDOS host is described, fetched, built,
installed, verified and updated. It is for administrators who want to know
what `kpkg` does to their machine, and for contributors who are about to add or
change a port and want to understand the machinery around the recipe.

It covers, in order: the ports tree, where source archives come from, the
package manager `kpkg`, what a package is, what a build checks, how the build
decides what to rebuild, reproducible packages, the signed binary host, binary
deltas, updating a machine, and vulnerability tracking.

Two neighbouring pages cover what this one does not:

- the recipe format itself, key by key, is in
  [Writing ports](../05-developer/writing-ports.md);
- applications (browsers, office suites and so on) are packaged by a separate
  system, described in [Packs and boxes](packs-and-boxes.md).

## The ports tree

A **port** is a directory describing one piece of software. It holds two files:

| File | What it is |
|---|---|
| `kpkgbuild` | Declarative metadata: name, version, sources, hashes, dependencies. Parsed, never run as a script |
| `build.sh` | The build. Ordinary bash, run with the working directory set to the unpacked source |

A port may also carry a `postinstall.sh` hook and `.patch` files. The upstream
archives a recipe names sit beside it once `make fetch` has run, but git does
not track them — see [Where sources come from](#where-sources-come-from).

The split into two files is deliberate. Because the metadata is parsed rather
than executed, reading a recipe runs no shell and cannot run anything. Because
the build is a real script, syntax checking, linting, highlighting and diffing
all work on it. See [Decisions](../01-philosophy/decisions.md).

There are three port repositories, searched in this order, all in one format:

```
/ports/core              1014 recipes — upstream software
/kdos/src/packages        11 recipes — ours: the tools, the installer, the packer
/kdos/src/desktop         13 recipes — ours: the compositor, the shell, the daemons
```

`src/packages` also holds `kdos-kpkg`, the package manager's own source, which
has no recipe: it is compiled by the phase 1 script `12_kpkg.sh` before any
recipe can be read.

`PORT_REPO` in each phase's environment file lists the repositories that phase
may resolve against. Phases 4 and 5 name the first two; the desktop phase names
all three. Building the desktop is therefore not a special case anywhere in the
build system.

## Where sources come from

Every upstream file a recipe uses is named by a `sha256 =` line in its
`kpkgbuild`. That hash is the file's identity: any copy that hashes to it is the
right file, wherever it came from. Git carries only the recipes.

`make fetch` (which runs `ports/fetch`) puts every hashed file into its port
directory and is **the only step that uses the network**. `make build` reads
nothing but port directories, so a tree fetched once builds offline
indefinitely. For each file, `ports/fetch` stops at the first copy that
verifies, looking in this order:

1. the port directory itself;
2. the local cache, `ports/.srccache/sha256-XX/<hash>` (`XX` is the hash's
   first two hex digits), hard-linked into the port directory;
3. the KDOS source archive,
   `https://github.com/kunaldawn/kdos/releases/download/sha256-XX/<hash>`;
4. the recipe's own `source =` URL upstream;
5. for a port's own vendor bundle only, generating it again.

The source archive is content-addressed and append-only: 256 GitHub releases
named `sha256-00` to `sha256-ff`, each asset named by its bare hash, and nothing
is ever replaced or removed. A five-year-old checkout therefore finds the exact
bytes it was written against even after the upstream host has gone. The
tree's recipes name 1,232 distinct files, 8.28 GiB, which is what a complete
archive holds; stored by hash, a file several ports use — the llvm monorepo
tarball is shared by eight — is one asset.

The useful commands and settings:

| Command or variable | Does |
|---|---|
| `make fetch` | Fetch and verify everything; generate missing vendor bundles in the `kdos-fetch` container |
| `make fetch-check` | Offline: list what is missing or corrupt, download nothing |
| `ports/fetch [port…]` | Fetch only the named ports |
| `ports/fetch --tree <dir> [port…]` | Fetch for another checkout's `ports/core` |
| `KDOS_SOURCES_BASE=` (empty) | Skip the archive and go straight to upstream |
| `KDOS_SOURCES_REPO` | The archive repository (default `kunaldawn/kdos`) |
| `KDOS_SRCCACHE` | Move the cache, for example to share one between checkouts |
| `KDOS_FETCH_HOST=1` | Do everything in one pass on this host, generating bundles with its own toolchains |

An empty file or a hash mismatch is always refused, and a corrupt cache entry is
deleted. A file whose recipe carries no hash yet — you have just bumped
`version =` — can only come from upstream.

Adding a new source to the archive is the contributor's side of this:
`ports/publish` uploads it, and an opt-in pre-push hook refuses a push naming a
hash the archive does not hold. Both are described in
[Writing ports](../05-developer/writing-ports.md#publishing-sources); the whole
fetch flow, step by step, is in
[Developing](../05-developer/developing.md#where-sources-come-from).

## kpkg

`kpkg` is one binary answering to five names; what it does depends on the name
it was run as:

| Name | Does |
|---|---|
| `kpkg` | The front end, with the commands below |
| `kpkgadd` | Install a prebuilt package file |
| `kpkgbuild` | Build a port into a package without installing it |
| `kpkgdel` | Remove a package |
| `kpkgdepends` | Print the resolved install order, and nothing else |

The `kpkg` front end's commands:

| Command | Does |
|---|---|
| `install <pkg>…` (`i`) | Build and install packages with their dependencies |
| `remove <pkg>…` (`r`) | Remove packages |
| `list` (`l`) | List installed packages |
| `info [--json] <pkg>` | Show a package's information |
| `meta <pkg>` | Print the recipe's metadata as shell assignments, which `ports/fetch` reads |
| `verify <pkg>` | Build with `kpkgbuild` and `kpkgbuild.new`, then compare the two packages |
| `verify --repro <pkg>` | Build the same recipe twice; the two packages must be byte-identical |
| `keygen <name>` | Make an Ed25519 signing key pair |
| `index <dir> [--sign <key>]` | Write `PACKAGES` for a directory of packages, optionally signing it and every package |
| `verify-index <dir>` | Check `PACKAGES` against the trusted keys |
| `verify-pkg <file>` | Check `<file>.sig` against the trusted keys |
| `delta <old> <new>` | Write the difference between two packages |
| `apply-delta <old> <delta> -o <new>` | Rebuild a package from a delta |
| `binhost <dir> <pkg> [--dry-run] [--insecure]` | Install a prebuilt package if it matches this machine exactly |
| `help` | Print usage |

Options and environment:

| Option or variable | Effect |
|---|---|
| `--root <path>`, `KPKG_ROOT` | Operate on another root directory, such as an installer's target |
| `--keep-cache` | Keep cached packages after installation |
| `-f`, `--force` | Rebuild the named packages and skip the file-conflict scan for them |
| `--overwrite`, `KPKG_OVERWRITE=1` | Let a package take a path another package owns; ownership moves with the file |
| `KPKG_CONF` | Configuration file (default `/etc/kpkg.conf`) |
| `KPKG_KEYRING` | Trusted-key directory (default `/etc/kdos/keys`) |
| `KPKG_REQUIRE_SIG=1` | Refuse any package without a valid signature |
| `KPKG_STRICT_RECIPE=1` | Skip only packages whose recipe hash is current; see [Deciding what to rebuild](#deciding-what-to-rebuild) |

`/etc/kpkg.conf` sets `PORT_REPO` (default `/ports/core`), `SOURCE_DIR`
(`/var/cache/kpkg/sources`), `PACKAGE_DIR` (`/var/cache/kpkg/packages`),
`WORK_DIR` (`/var/cache/kpkg/work`) and `PKGDB_DIR` (`/var/lib/kpkg/db`); each
may be overridden by an environment variable of the same name.

`kpkgdepends` writes one bare, space-separated line to standard output and
nothing else, because the build orchestrator parses it. Diagnostics from
anything running inside the build chroot go to a log, and every token the
orchestrator reads is checked against a strict pattern, so stray output fails
loudly instead of being installed as a package.

`kpkg` links three of this tree's libraries — `libkbase`, `libkpkg` and
`libksig` — and nothing else, so it is cross-compiled early and exists on every
tree from the first bootable image onward.

## Packages

A package is a compressed tar archive plus a database entry.

| | |
|---|---|
| File name | `<name>-<version>-<release>.tar.xz` |
| Database | `/var/lib/kpkg/db/<name>` |
| Manifest | Every path the package owns, `./`-prefixed, directories with a trailing slash |
| Install hook | `.POSTINSTALL` inside the archive, when the port has a `postinstall.sh` |

### Who owns a file

A file conflict is between *packages*. A path that exists but that no installed
package claims is **adopted**, not refused. This is what makes the bootstrap
work: the earliest build phases install a toolchain by hand, leaving files no
database entry owns, and the bootstrap then rebuilds exactly those packages with
`kpkg`.

A path that another package **does** own is still a conflict. Where the userland
genuinely overlaps — toybox, the compact userland, ships many names that the
full GNU tools also provide — whoever comes last in dependency order wins, using
the explicit overwrite flag. That flag does one thing besides allowing the
write: the path changes hands in the database and is removed from the previous
owner's manifest. Without that, removing the older package would delete a file
the newer one installed.

Ownership is compared on the **canonical** path. The root's `bin`, `sbin`, `lib`
and `lib64` are links into `/usr`, so a package built with `--exec-prefix=`
records `./bin/free` for the same file another records as `./usr/bin/free`.
Compared as strings the two never collide: the scan passes, the file is claimed
twice, and removing or upgrading either package deletes the other's file. `kpkg`
reads which top-level names are such links from the root it installs into, and
folds each path through them before comparing.

### toybox and the tools it overlaps

toybox is built without every applet whose name another port on the image
installs, so each name has one owner and one implementation. The GNU tools the
bootstrap cannot do without — `sed`, `find`, `xargs`, `awk`, `expr`, `ln` — are
the exception: toybox keeps them, the GNU ports come after it in dependency
order and take them over, and upgrading toybox alone puts its applets back until
those ports are reinstalled.

Dropping one more applet from toybox without bumping the release of the port
that owns that name leaves the name missing after a toybox upgrade. A toybox
installed after the owning port holds the name in its manifest, its upgrade
removes the name, and `kpkg` skips the owner because its recipe hash is
current — so nothing puts the name back. A contributor who removes an applet
therefore bumps the owning port's `release` in the same change. The owners that
[phase 3](../05-developer/build-system.md#phases) (the toolchain and core
libraries) installs before toybox are listed straight after it in
[phase 4](../05-developer/build-system.md#phases) (the userland), so their names
return before a later build step runs them.

### Upgrades and removals

An upgrade removes orphans. A file present in the old version and absent from
the new one is removed rather than left on disk owned by nothing — unless
another package claims it, in which case it is that package's file and stays. A
removal follows the same rule.

An install or removal ends by rebuilding the shared indexes its manifest fed,
from everything then on disk: the GSettings schemas, the GIO module and pixbuf
loader caches, the MIME database, the font cache and the X core fonts'
`fonts.dir`, the info directory and the udev hardware database. The manual
index is merged instead: an install that only adds pages adds them to
`mandoc.db`, and the index is rebuilt from the whole tree only when a page was
removed (by a removal, or as an upgrade's orphan) or when there is no
`mandoc.db` yet. No package owns these files, so no package ships them; the
list is in [Writing ports](../05-developer/writing-ports.md#shared-indexes).

### Files kpkg keeps to itself

The system Python's `site-packages` belongs to `kpkg`. `python3` ships the PEP
668 `EXTERNALLY-MANAGED` marker, so `pip install` outside a virtual environment
is refused rather than allowed to replace a port's files, which the next upgrade
or removal of that port would delete or conflict with. To install Python
software that is not a port, create an environment with `python3 -m venv DIR`;
`python3-pip` ships the pip wheel that `ensurepip` installs into it.

`/etc/localtime` is kept out of every package for the same reason: a file the
machine's owner changes cannot also be a file an upgrade rewrites. A machine
whose installed `tzdata` manifest still lists it has the link removed as an
orphan on the next upgrade — see
[Known gaps](../06-reference/known-gaps.md#build-and-packaging).

## What a build verifies

Before it touches the work directory, `kpkgbuild` hashes **every** file a
`sha256 =` entry names that is present beside the recipe or in `kpkg`'s source
directory (`SOURCE_DIR`, default `/var/cache/kpkg/sources`), and refuses on any
mismatch.

That is wider than the `source =` list on purpose. 124 ports — the Go, Rust,
Python and Haskell ports, and pdfium — carry a **vendor bundle**: their
language dependencies, packed as
`<name>-vendor-<version>.tar.xz` and unpacked by `build.sh` itself. A vendor
bundle is declared with a hash but named by no `source =` line, so a check that
walked `source =` alone would compile those ports from bytes nothing had looked
at.

A declared file that is in neither place is skipped rather than refused. A
source the build actually needs is caught when extraction cannot find it, and
failing on a declared file the build never opens would refuse a port over a
hash that cannot affect it.

On top of that, no source is unpacked before its bytes match. A source the
recipe names with no `sha256 =` for it is a hard failure, not a warning.
`KDOS_ALLOW_UNVERIFIED=1` is the escape hatch for bringing up a new port before
its hash is known, and `testing/preflight.sh` checks that no recipe in the tree
needs it.

The first `source =` entry is looked for under the standard name
`<name>-<version>.<ext>` (for example `zlib-1.3.1.tar.gz`), which is the name
`ports/fetch` saves it under; later entries keep their URL's basename, and
`file::url` names a file explicitly.

## Deciding what to rebuild

The build must not recompile 1,038 ports on every run, and must not skip one
whose recipe changed. Two hashes decide, and they are the same two the binary
host uses.

### `E:` — the recipe hash

SHA-256 over `kpkgbuild`, `build.sh`, `postinstall.sh` and every `.patch` file,
sorted by name, each contributing its name, its length *and* its bytes. It is an
exact statement of what a package was built from.

For a port that names a `source =`, nothing else in the directory is hashed: a
tarball or a vendor bundle is covered by its own `sha256 =` line, which
`kpkgbuild` checks before it builds anything (see
[What a build verifies](#what-a-build-verifies)). A file beside the recipe that
no `sha256 =` names is in neither this hash nor that check.

A port with no `source =` is different, and this is what keeps the rule true for
this tree's own code. Such a port builds out of its own directory: nothing names
those files and no checksum covers them. If only the four recipe files were
hashed, editing a `.c` file would change nothing the build can see — the port
would report as installed and current, and the tree would keep the binary it
already had. The symptom would never be a build error; it would be a shipped
program behaving like an older one.

So a source-less port hashes its **whole directory**, sorted at every level,
**and all of `src/libs` with it**. Each `build.sh` names which libraries it
compiles, and working that out would need a shell parser inside the package
manager, so every library is included.

The cost: editing one library rebuilds every port of ours, not only its
consumers. Upstream ports' hashes are unaffected.

### `B:` — the build-config hash

The architecture, the C library (always musl), the target triple, the compiler
and its version, and the C, C++ and linker flags. Two machines with the same
`B:` produce comparable binaries; two with different `B:` do not, whatever the
recipe says.

Both hashes include their field **names**, because a hash over values alone
collides the moment two fields swap.

### The three states

When a package is already installed, `kpkg` compares its recorded recipe hash
with the port as it stands:

| State | Result |
|---|---|
| Hashes match | Skip |
| Hashes differ | Rebuild |
| No recorded hash, or a corrupt one | **Skip** |

The third row makes the check safe on a tree that has packages without a
record: absent reads as *unknown*, never as *changed*, so no package is rebuilt
merely for lacking a record. A record that is not exactly the right shape is
treated the same way; reading it as a mismatch would rebuild that one package on
every run, forever, with nothing saying why.

The hash is recorded **after** a successful install, never before. A record
written ahead of a build that then fails would claim a recipe is installed that
is not.

`KPKG_STRICT_RECIPE=1` turns the check on. Every build phase's environment sets
it, so the build is strict; an interactive `kpkg install` is not.

The dependency solver applies the check, not the install loop. An installed and
current package is dropped before the loop runs, so a check placed later would
reach only packages named on the command line and miss every *dependency* whose
recipe changed.

## Reproducible packages

A package built twice from the same tree is byte-identical. That is a property
of one function — the archive roller inside `kpkg` — rather than of 1,038
recipes, which is why `kpkg` rolls the archive itself instead of letting each
`build.sh` do it.

Each setting removes one source of difference between two builds:

| Setting | Without it |
|---|---|
| `--sort=name` | Directory order is filesystem order, which is not stable even between two copies of the same tree |
| `--mtime=@$SOURCE_DATE_EPOCH` | Every file carries the second it was installed |
| `--owner=0 --group=0 --numeric-owner` | The builder's user id, and its *name* as text in the header |
| `--format=gnu` | Extended headers carry access and change times, which are wall clock |
| `--use-compress-program=xz -9 -T1` | Multi-threaded compression is not deterministic, and `XZ_OPT` in the environment can silently enable it |
| `umask(022)` before the build | A file created without an explicit mode takes the builder's umask — the one source of drift that is not in the archive call |

The other half is five lines in every phase's environment file:

| Line | Purpose |
|---|---|
| `SOURCE_DATE_EPOCH=1735689600` | A pinned epoch, not the current date and not derived from git, which the build container does not have |
| `TZ=UTC` | Dates formatted during the build do not depend on the builder's zone |
| `LC_ALL=C` | Sorting and formatting do not depend on the builder's locale |
| `-ffile-prefix-map=/var/cache/kpkg/work=/build` | Rewrites the build directory out of `__FILE__` and debug paths |
| `-Wl,--build-id=sha1` | The build identifier is a function of the contents, not random |

Reproducibility is what makes a signed binary host meaningful, what lets a delta
rebuild a package that still verifies against the *original* signature, and
what lets a rebuild be checked against what it was built from (`kpkg verify
--repro`).

## The binary host

A **binary host** (binhost) is a directory of prebuilt packages with a signed
index. It is optional: KDOS builds everything from source, and a binhost only
saves compiling. It is a path, not a URL — a USB stick, an NFS mount or any
other directory.

```sh
kpkg keygen builder                      # once, on the machine that builds
kpkg index /repo --sign builder.key      # PACKAGES + PACKAGES.sig + a sidecar per package
cp builder.pub /etc/kdos/keys/           # on every machine that should trust it
kpkg binhost /repo zlib                  # install it, or say why it will not
```

The index is a flat text file: single-character keys, one stanza per package, a
blank line between stanzas. It parses in a few dozen lines of C and reads fine
in a pager.

Three equality tests decide whether a prebuilt package is usable, and there is
no "close enough": the architecture, the build-config hash `B:`, and the recipe
hash `E:`. Anything else builds from source. The exit status says which
happened:

| Exit | Meaning |
|---|---|
| 0 | The prebuilt package was used |
| 1 | No match — build it from source (also: a usage error) |
| 2 | Refused: no index, no trusted key, or verification failed |

This replaces Gentoo's whole USE-flag matching problem with equality tests, and
it works because KDOS has no USE flags.

### Signing

Signatures are Ed25519, through a vendored public-domain implementation
(Monocypher) — the only third-party source under `src/libs`. See
[The C libraries](../05-developer/c-libraries.md).

One signature over the index covers every package, because the index carries
each package's hash. A per-package sidecar signature exists for the separate
case of a package travelling on a stick with no index beside it.

The scheme keeps these rules:

- **A key id is a label, not a selector.** Every signature line is tried against
  every key in the trusted directory, and against nothing outside it. Tools
  report the key that verified, not the id the line claimed, and a line naming
  an unknown id still verifies if a trusted key signed it. A signature that
  could supply its own key would verify nothing.
- **A bad signature is not a missing one.** Installing a package whose sidecar
  *fails* is refused. One with no sidecar is allowed, because locally built
  packages are the majority and are never signed. `KPKG_REQUIRE_SIG=1` is the
  stricter policy for a machine that installs only from a binhost.
- **`--insecure` says so every time it is used**, so it is never left quietly in
  a script.
- **Verify before use.** The index is verified before it is believed, and a
  package's hash before it is unpacked.
- **Several signatures are allowed.** A signature file is one line per
  signature, so during a key rollover both keys sign and a client trusting
  either keeps working.
- **The private key is protected.** It is written with restrictive permissions
  and exclusive creation, and reading it back is refused if its mode has
  loosened.

The trusted directory *is* the policy. There is no revocation list and no online
check: trusting a key is copying a file in, and removing trust is deleting it.
The loader does **not** descend into subdirectories, which keeps
`/etc/kdos/keys` (host packages) and `/etc/kdos/keys/packs` (application packs)
separate policies.

Ports built from source are not signed and need no signature. A port is compiled
on your machine and its integrity is the `sha256 =` in its recipe. Signing
answers "who made this binary", and for a port the answer is "you did".

## Deltas

```sh
kpkg delta zlib-1.3-1.tar.xz zlib-1.3.1-1.tar.xz
kpkg apply-delta zlib-1.3-1.tar.xz <delta> -o zlib-1.3.1-1.tar.xz
```

A delta is a binary difference between two packages, so a machine that already
has the old version downloads kilobytes instead of the whole package. The
engine is `zstd --patch-from`; two decisions around it are the whole design.

**The delta is taken over the uncompressed archives.** Two compressed files
built from nearly identical trees share almost no bytes — that is what a
compressor does — so a delta between them is as large as the package.
Decompressing first is the difference between a few kilobytes and the full
package.

**A delta is never trusted, and never needs to be.** It is applied, and the
**result** is hashed against the entry the signed index already carries. A
tampered delta produces a package whose hash does not match, which is discarded;
it cannot make a client install anything the index did not already name. So
there is no delta signature and no second trust path.

A delta appears in the index as an ordinary stanza with one extra key naming the
package file it applies to. A client uses one only when it still has that old
package, which is the ordinary case for a machine that updates regularly.

## Updating a machine

`kdos update` is the command a person types; it drives `kpkg` and `kpkg binhost`
and adds no new trust path.

| Command | Does |
|---|---|
| `kdos update check` | Compare what is installed with what the ports tree on this machine pins |
| `kdos update apply` | Install what is behind: from the binhost where it has a match, compiled otherwise |
| `kdos update theme` | Re-run the theme generators for your home directory after an artwork upgrade |

New versions arrive with the **ports tree**, not with the binhost: a recipe pins
its version, so a binhost package that matches this machine is by construction
the version the tree would build. A machine with no ports tree therefore cannot
update. A stick built with `KDOS_ISO_SOURCES=1` carries one.

The binhost is named by `binhost = /path/to/repo` in `/etc/kdos/update.conf`, or
by `$KDOS_BINHOST`. On a machine with A/B root slots, `apply` installs into the
mounted inactive slot and marks it to be tried on the next boot, so the running
system is untouched and a bad update rolls back; `--in-place` updates the
running root instead. Every option is in
[the kdos command](../04-programs/kdos-command.md#kdos-update).

## Vulnerability tracking

```sh
kdos cve
```

The question is a version comparison, not a scan: *is the version we pin older
than the version the database says fixed this issue?* The comparison is the
package manager's own version comparison, shared with the upstream-version
checker (`ports/update`) so the two cannot disagree about what "newer" means.

The data is a vendored, pruned copy of Alpine's security database, installed as
`/usr/share/kdos/secdb.txt` from `src/packages/kdos-tools/secdb/secdb.txt`. It
is a committed, diffable text file merged from twelve Alpine branches (`main`
and `community` for v3.19 to v3.24), so the answer needs no network. See
[Decisions](../01-philosophy/decisions.md) for why Alpine rather than a larger
source.

Four details each change the answer:

- **Alpine's packaging revision is stripped** before comparing. Leaving it on
  makes every pin look old.
- **"Fixed in 0" means never affected** in that branch, and falls out of the
  comparison for free.
- **The newest fix a pin is behind is the one reported**, because it closes
  every earlier one too, and the identifiers from all matching rows are merged.
- **A `secdb =` key in a recipe** maps a port whose name differs from Alpine's.

A package the database does not carry is reported as UNKNOWN, never as clean. A
large part of the tree is in that state and the summary says so. The database's
age is printed with every run, with a warning once it is more than 180 days old.

`ports/update --cve` is the online cross-check: one request per port against a
rate-limited service. That cost is why the vendored table is the everyday
answer.

## See also

- [Writing ports](../05-developer/writing-ports.md) — the recipe format and how to add one
- [Developing](../05-developer/developing.md) — fetching sources and running the build
- [The build system](../05-developer/build-system.md) — how phases drive `kpkg`
- [Packs and boxes](packs-and-boxes.md) — the other packaging system, and why it is separate
- [The security model](security-model.md) — the trust argument behind the keyrings
- [The kdos command](../04-programs/kdos-command.md) — `kdos cve` and `kdos update`

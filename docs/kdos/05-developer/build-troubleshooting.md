# Build troubleshooting

This chapter is for anyone whose KDOS fetch, build or push has failed: a contributor adding or
bumping a port, or someone building the system from source for the first time. Most failures in
this repository are one of a few dozen recurring problems, and many of them report something
other than what is wrong: a missing type that is an empty generated header, a compiler that "cannot
create executables" when the compiler is fine. The chapter catalogues those problems by the message
you will see, and for each gives the cause and the fix that works in this repository. It assumes you
know how a build is run ([Developing](developing.md)) and what a recipe looks like
([Writing ports](writing-ports.md)); [The build system](build-system.md) explains the phases, the
chroot and the orchestrator that the entries below refer to, and
[How KDOS is built](how-kdos-is-built.md) tells the whole build as one story for a first read.

Start with [Reading a failure](#reading-a-failure), then find your message in the
[symptom index](#symptom-index) and follow its link. The entries are grouped by where the failure
arises: fetching and publishing sources, the orchestrator and the build tree, toolchains and
language runtimes, the compact userland, and upstream build systems. If nothing matches, work
through [When the failure is not here](#when-the-failure-is-not-here) at the end.

## Reading a failure

A build is a sequence of steps, and every step writes its whole output to its own log:

```text
build/logs/<phase directory>/<NNNN>_<name>.log
```

For a script step, `<name>` is the script's file name without its numeric prefix
(`build/logs/01_phase1/0000_file_system.sh.log`); for a port built from a phase's package list it is
the port name followed by `.install` (`build/logs/04_phase4/0123_mesa.install.log`). The
orchestrator names the failing step, and in its failure panel `O` opens the log and `C` copies the
path. The message at the very end of a build's output is usually not the error: read upward from the
end of the log. Three other files sit under `build/logs/`: `snapshots.log` (every notice the build
showed), `chroot.log` (mount warnings from `script/chroot_exec.sh`) and `<phase>/expansion.log` (why
a phase's package list could not be resolved). [Step logs](build-system.md#step-logs) has the
details.

A few terms recur throughout. `kpkg` is KDOS's package manager: it builds each port from its
recipe and installs it, and `kpkgdepends` is its dependency solver. KDOS uses musl as its C library
in place of glibc, and toybox, a single binary that provides most of the standard command-line tools
as *applets*, as its base userland. *Preflight* is `testing/preflight.sh`, a static check of the
repository's wiring. The *target tree* is `build/fs`, the root filesystem the build assembles and
every phase from phase one onward installs into. The [Glossary](../06-reference/glossary.md)
defines the rest.

When a port's own build fails, `kpkg` leaves its work directory in place: the unpacked source is
at `/var/cache/kpkg/work/<name>/<name>-<version>/` inside the target tree, which is
`build/fs/var/cache/kpkg/work/…` on the host. A configure script's `config.log` is there. The
target tree is owned by root, as the shipped system is, so reading it from the host needs a
container or `sudo`.

Two checks run without a build and catch whole classes of failure before one starts:

```sh
testing/preflight.sh     # the wiring: dependencies, hashes, meson options, recipe syntax
make fetch-check         # offline: every archived source present and matching its hash
```

`testing/preflight.sh` takes two to three minutes. Run it after `make fetch`, because several of
its checks read the fetched tarballs and skip a port whose tarball is absent. `make fetch-check`
hashes every archived source, which takes about a minute. [preflight.sh](testing.md#preflightsh)
lists everything preflight checks.

## Symptom index

| What you see | Section |
|---|---|
| `Source not found: <file> (checked <port directory> and /var/cache/kpkg/sources)` | [A source that was never fetched](#a-source-that-was-never-fetched) |
| `sha256 MISMATCH for <file> from <url>` from `ports/fetch`, or `sha256 MISMATCH for <file>` from `kpkg` | [A downloaded source that fails its hash](#a-downloaded-source-that-fails-its-hash) |
| `No sha256 for <file> in the recipe — refusing to extract an unverified source` | [A source with no hash](#a-source-with-no-hash) |
| Preflight: `declares <file>, which is neither in the port directory nor hashed`, or `ships <file> with no sha256 line` | [A source with no hash](#a-source-with-no-hash) |
| Preflight: `ships <file>, whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or tar`, or `ships <file> as an empty file` | [An error page saved as a tarball](#an-error-page-saved-as-a-tarball) |
| Preflight: `recipe-hashed archives are tracked by git`, or `make fetch-check` counting fewer sources than the recipes name | [Sources still in the git index](#sources-still-in-the-git-index) |
| `sha256 MISMATCH for the generated <name>-vendor-<version>.tar.xz` | [A sha256 mismatch on a vendor bundle](#a-sha256-mismatch-on-a-vendor-bundle) |
| `cannot generate <bundle> without its source`, `this needs docker or podman`, `failed to build the recipe reader`, or `--tree never generates` | [A vendor bundle that cannot be generated](#a-vendor-bundle-that-cannot-be-generated) |
| `pre-push: these sources are named by a recipe but not in the archive at that commit` | [Pre-push refused: an unpublished source](#pre-push-refused-an-unpublished-source) |
| `Failed to download` for every file, `is in no cache, and the archive is off`, or `pre-push: cannot reach the source archive` | [Fetch cannot reach the archive](#fetch-cannot-reach-the-archive) |
| `package resolution failed - see build/logs/.../expansion.log` | [A package list that does not resolve](#a-package-list-that-does-not-resolve) |
| `build/iso-build/kdos.iso is open by another process` | [The ISO is in use](#the-iso-is-in-use) |
| `snapshot <phase> FAILED: only <size> free, need ~<size>` | [A snapshot the orchestrator refuses](#a-snapshot-the-orchestrator-refuses) |
| `snapshot <phase> FAILED: build/ is mid-restore of <phase>`, or `a restore of <phase> never finished` | [A snapshot the orchestrator refuses](#a-snapshot-the-orchestrator-refuses) |
| `snapshot <phase> FAILED: mounts still active under build/fs: <paths>` | [A snapshot the orchestrator refuses](#a-snapshot-the-orchestrator-refuses) |
| A port you changed is reported `Skipping <port> (already installed)` | [A recipe change that did not rebuild its port](#a-recipe-change-that-did-not-rebuild-its-port) |
| A variable passed to `make build` has no effect on a chroot phase | [A variable that does not reach the chroot](#a-variable-that-does-not-reach-the-chroot) |
| `Removing orphan` for a file another package still lists, then `not found` on that tool | [A package manager older than its source](#a-package-manager-older-than-its-source) |
| `cmp`, `readelf` or another tool missing after toybox reinstalls | [A tool missing after toybox reinstalls](#a-tool-missing-after-toybox-reinstalls) |
| Phase-one files over a later build, or a later tree filed under phase one's snapshot | [Re-running an early phase on a later tree](#re-running-an-early-phase-on-a-later-tree) |
| A `ports/` helper that fails to start after you ran it in a container | [Helpers compiled against the other C library](#helpers-compiled-against-the-other-c-library) |
| `Dynamic loading not supported` from a Rust crate, or a static archive's undefined references at its link | [Rust with a binding generator](#rust-with-a-binding-generator) |
| `'cstddef' file not found` from clang or bindgen, in a header that compiles with gcc | [An LLVM that guessed its triple](#an-llvm-that-guessed-its-triple) |
| `'+amx-tf32' is not a recognized feature for this target` from rustc | [A Rust release older than the system LLVM](#a-rust-release-older-than-the-system-llvm) |
| `rustc <version> is not supported by the following packages` | [A crate newer than the toolchain](#a-crate-newer-than-the-toolchain) |
| `no matching package named …` with the vendor tree present | [A vendor bundle in the wrong place](#a-vendor-bundle-in-the-wrong-place) |
| CMake being compiled from source during a download step | [A Python backend resolving a system tool](#a-python-backend-resolving-a-system-tool) |
| `unknown type name 'bool'` inside a GCC target header, while building libgcc | [A language standard reaching the compiler's own runtime](#a-language-standard-reaching-the-compilers-own-runtime) |
| `'fenv_t' has not been declared`, then `Cannot compile std module`, in phase one | [The installed C++ headers shadowing the ones being built](#the-installed-c-headers-shadowing-the-ones-being-built) |
| `undefined reference to libintl_gettext` | [Gettext on musl](#gettext-on-musl) |
| A wide-character curses function as an implicit declaration | [The wide curses API](#the-wide-curses-api) |
| `ubrk_*` missing at link | [An ICU component not propagated](#an-icu-component-not-propagated) |
| A missing type, from an empty generated header | [A stream-editor extension that is not there](#a-stream-editor-extension-that-is-not-there) |
| `integer expected` from `expr`, or a relative-link option rejected | [Missing compact-userland features](#missing-compact-userland-features) |
| A package index or a clone reached during the offline build | [A build that reaches the network](#a-build-that-reaches-the-network) |
| `Unknown options: …` at meson setup | [An unknown meson option](#an-unknown-meson-option) |
| A meson option value that `is not one of the choices` | [A meson feature given a boolean](#a-meson-feature-given-a-boolean) |
| `Error loading shared library` at run time, or a stale package-config file shadowing a fixed one | [A missing meson prefix or library directory](#a-missing-meson-prefix-or-library-directory) |
| `Compatibility with CMake < 3.5 has been removed` | [An old CMake policy floor](#an-old-cmake-policy-floor) |
| An option you passed had no effect, with a warning about unused variables | [A misspelt CMake option](#a-misspelt-cmake-option) |
| Every static link failing on unwinder symbols | [A CMake file with no project declaration](#a-cmake-file-with-no-project-declaration) |
| `Skipped dir` on a path that looks like the binary | [Go building into its own directory](#go-building-into-its-own-directory) |
| `C compiler cannot create executables` | [`C compiler cannot create executables`](#c-compiler-cannot-create-executables) |
| The same, with a working compiler on the search path | [A configuration script preferring another compiler](#a-configuration-script-preferring-another-compiler) |
| `error: incompatible pointer types` | [Newer-compiler diagnostics as errors](#newer-compiler-diagnostics-as-errors) |
| Warnings you have never seen upstream, made fatal | [An upstream `-Werror`](#an-upstream--werror) |
| An undeclared constant that reads like a missing header | [Compiler flags passed as make arguments](#compiler-flags-passed-as-make-arguments) |
| `No rule to make target` printed from the middle of an unrelated step | [A backtick inside double quotes](#a-backtick-inside-double-quotes) |
| `No rule to make target '\'` during an install | [A parallel install race](#a-parallel-install-race) |
| A BSD header missing on a fresh build only | [A dependency the list happened to satisfy](#a-dependency-the-list-happened-to-satisfy) |
| GStreamer's core compiling or failing in a Rust helper (`gst-ptp-helper`) | [The time-protocol helper](#the-time-protocol-helper) |
| A submodule directory present but empty | [An empty submodule in a release archive](#an-empty-submodule-in-a-release-archive) |
| A downloaded archive that does not exist, or unpacks oddly | [Source URLs and archive layouts](#source-urls-and-archive-layouts) |

---

## Sources, fetching and publishing

Upstream sources are not in git. A recipe names each source by its `sha256 =` line, and
`make fetch` (which runs `ports/fetch`) resolves every hash from the first place that has a copy
that verifies: the port directory, the local cache `ports/.srccache`, the `kunaldawn/kdos` source
archive (located through the committed index `ports/sources.idx`), and finally the URL in the
recipe's `source =` line. `make fetch` is the only networked step. `make build` runs its container
with `--network none` and mounts `ports/` read-only, so the build can only read what the fetch has
already put beside each recipe. [Where sources come from](developing.md#where-sources-come-from)
and [Publishing sources](writing-ports.md#publishing-sources) describe the whole arrangement.

### A source that was never fetched

```text
Source not found: <file> (checked <port directory> and /var/cache/kpkg/sources)
```

`kpkg` reads sources only from the port directory and its own source directory,
`/var/cache/kpkg/sources` (`SOURCE_DIR` in `/etc/kpkg.conf`). A fresh clone, a recipe edited since
the last fetch, or a branch switch that changed a version all leave a port without its archive.
`make fetch-check` lists every archived source that is missing or fails its hash, without touching
the network, and names the fix:

```sh
make fetch-check
make fetch                   # or, for one port: ports/fetch <port>
```

If the file is on disk under another name, the recipe names it differently from how it was saved.
A `file::url` entry is saved as `file`. The first `source` of a recipe, when its URL ends in a
recognised archive suffix (`.tar.gz`, `.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst`,
`.zip`), is saved as `<name>-<version>.<ext>` whatever the URL's own file name is, with `.tgz`,
`.tbz2` and `.txz` written out as `.tar.gz`, `.tar.bz2` and `.tar.xz`; every other entry keeps the
URL's file name. See
[Sources, and what each one becomes](writing-ports.md#sources-and-what-each-one-becomes). Preflight
reports a stray file as one `which no 'source =' line resolves to`.

### A downloaded source that fails its hash

```text
✗  <port>: sha256 MISMATCH for <file> from <url>
   expected <recipe hash>
   got      <hash of what arrived>
```

`ports/fetch` deletes a download that does not match the recipe and tries the next location, so
this line on its own is not a failure. A resumed download that fails its hash is first asked for
once more from zero, from the same URL. The port fails only when every location has given the
wrong bytes.

`kpkg` prints `sha256 MISMATCH for <file>` at build time. It checks every file a `sha256 =` line
names that is present in the port directory, not only the ones a `source =` line reaches, so the
usual cause is a recipe whose hash was changed after the file beside it was fetched. Run
`ports/fetch <port>` again: it replaces a file that does not match.

When every location disagrees with the recipe, upstream has changed the file under the same name;
forges regenerate archives, and some projects re-roll a release. If the source was ever published,
the archive still holds the bytes the recipe names, so check that the archive is reachable (see
[Fetch cannot reach the archive](#fetch-cannot-reach-the-archive)). If it never was, compare the
new file with upstream's announcement or signature before you trust it, then replace the
`sha256 =` line and [publish](writing-ports.md#publishing-sources) the file.

### A source with no hash

```text
No sha256 for <file> in the recipe — refusing to extract an unverified source (KDOS_ALLOW_UNVERIFIED=1 to override)
```

or, from preflight, `declares <file>, which is neither in the port directory nor hashed`, or
`ships <file> with no sha256 line`.

Every source a recipe names needs a `sha256 = <hash>  <file>` line, matched by file name. `kpkg`
refuses to extract a file with none, and `ports/fetch` can fetch such a file only from its upstream
URL, never from the cache or the archive, and warns `no sha256 for <file> in the recipe`. The usual
cause is a version bump whose `sha256 =` lines still name the old file: a bump made by hand, with
`ports/update --no-fetch`, or as a group bump, which rewrites the versions of every member but not
their hashes. A single-port bump through `ports/update` rewrites the hash itself. Fetch the new
file and record its hash:

```sh
ports/fetch <port>
sha256sum ports/core/<port>/<file>
```

`KDOS_ALLOW_UNVERIFIED=1` lets a build extract an unhashed source while you bring a port up.
Preflight fails if any `kpkgbuild` mentions the variable, so it cannot stay in a recipe.

### An error page saved as a tarball

Preflight reports `ships <file>, whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or
tar`, or `ships <file> as an empty file`; without preflight, the build fails at
`tar: Error is not recoverable`.

A mirror that answers a download with an error page writes HTML under the tarball's name, and an
interrupted download can leave an empty file. A hash recorded from either verifies,
since it is the hash of those bytes. Delete the file, fetch it again from a working URL, and
replace the `sha256 =` line with the hash of the real archive. `ports/fetch` itself refuses an
empty file as a download, so this arises from a hash recorded by hand.

### Sources still in the git index

Preflight fails with `<N> recipe-hashed archives are tracked by git — git rm --cached them`, and
`make fetch-check` counts fewer archived sources than the recipes name, down to
`All 0 archived sources present and verified` when none are left.

`ports/fetch` treats every file in the git index as carried by git and skips it: it neither
fetches nor checks it. A source tarball still in the index, for example as a Git LFS pointer, is
therefore invisible to `make fetch` and `make fetch-check`, while `ports/publish` and the pre-push
hook treat an LFS pointer as a file to archive. Take the tarballs out of the index (the files on
disk stay where they are) and commit that change:

```sh
git rm --cached ports/core/<port>/<file>
```

`make fetch-check` then counts and verifies them, and `.gitignore` keeps them out of the index.
Preflight also fails if `.gitignore` stops covering any source suffix or `ports/.srccache/`.

### A vendor bundle that cannot be generated

A *vendor bundle*, `<name>-vendor-<version>.tar.xz`, holds the crates, modules or packages a port's
language would otherwise download during the build (see [Vendoring](writing-ports.md#vendoring)).
`ports/fetch` generates one only when no copy that verifies exists in the port directory, the
cache or the archive. Generating needs the language toolchains at the versions the recipes name, so
by default it runs in the `kdos-fetch` container, built from `ports/Containerfile.fetch`. These
messages mean it could not:

| Message | Cause and fix |
|---|---|
| `cannot generate <bundle> without its source` | Another of the port's files, usually its main tarball, also failed to download, and the bundle is generated from it. Fix that download first |
| `ports/fetch: this needs docker or podman` | Neither container engine is on `PATH`. Install one, or set `KDOS_FETCH_HOST=1` to generate with this machine's own `cargo`, `go`, `npm`, `pip` or `cabal`, which then have to be the versions the recipes name |
| `failed to build the recipe reader` | `ports/fetch` compiles `kpkg` on the host to read recipes (`ports/.kpkgbin/kpkg`), and there is no C compiler (`$CC`, default `cc`). A plain fetch hands the whole run to the container instead; `--check`, `--tree` and `KDOS_FETCH_HOST=1` need the host compiler |
| `--tree never generates` | `ports/fetch --tree <dir>` fetches for another checkout and does not generate bundles. Run that checkout's own `ports/fetch <port>` |

### A sha256 mismatch on a vendor bundle

```text
✗  <port>: sha256 MISMATCH for the generated <name>-vendor-<version>.tar.xz
   expected <recipe hash>
   got      <hash of what was generated>
```

A generated bundle is held to the recipe's hash like a download. `ports/fetch` packs it with
sorted names, a fixed time stamp and fixed ownership, so the same resolution produces the same
bytes twice; a mismatch means the resolution itself differed from the one the hash records: a
registry that moved, a yanked crate, a toolchain version that changed the lock file. The generated
file is deleted and the port fails, because keeping it would put a bundle nothing verifies in
front of the build.

If the bundle was ever published, it is in the archive and the fetch would not have generated it;
check that `KDOS_SOURCES_BASE` is not empty and the archive is reachable. If it never was, the
recipe's hash names bytes that exist nowhere. Replace the `sha256 =` line with the hash printed on
the `got` line, build the port, and run `ports/publish <port>` so every other clone fetches that
bundle instead of generating its own.

### Pre-push refused: an unpublished source

```text
pre-push: these sources are named by a recipe but not in the archive at that commit:
  <port>/<file>  (<hash>)
pre-push: publish them and commit the index first:  ports/publish <port> && git commit ports/sources.idx
```

The pre-push hook (`script/hooks/pre-push`, enabled with `git config core.hooksPath script/hooks`)
checks every source hash the pushed commits add. Each must have a line in `ports/sources.idx` as of
the pushed commit, and the archive release that line names must hold the file. A line reading
`<port>/<file> — not in ports/sources.idx` means the index line is missing: the upload may have
happened, but the index change was not committed, and without it `ports/fetch` cannot find the
file. Run the command the hook prints, which uploads from the port directory or the cache and
writes the index lines, commit the index, and push again.

Uploading needs a token with write access to the archive; the token, pacing and flags are in
[`ports/publish`](writing-ports.md#portspublish), and what the hook checks is in
[The pre-push hook](writing-ports.md#the-pre-push-hook). Without that access, push with the check
bypassed and name the ports in your pull request, so a maintainer publishes them:

```sh
KDOS_SKIP_PUBLISH_CHECK=1 git push …
```

### Fetch cannot reach the archive

`ports/fetch` asks the source archive before upstream, so an unreachable archive shows as every
archived source falling through to its upstream URL: each file prints a download line for the
archive and then one for upstream. That is slower, and fails outright with
`Failed to download <file>` for any file whose upstream has gone. An archive that answers 404 for a
file behaves the same way for that file: either it was never published, or the archive repository
is not publicly readable. With the archive switched off and no upstream URL for a file, the message
is `<file> is in no cache, and the archive is off and no source names it`.

The pre-push hook refuses rather than passing, since absence cannot be ruled out offline:
`cannot reach the source archive at <base> (curl exit N)`, or
`source archive unreachable at <base> (HTTP N)` when the archive answers anything but 200 or 404.
`ports/publish` stops the same way.

Check that the machine can reach `https://github.com` at all. Then look at the variables
`ports/fetch`, `ports/publish` and the hook all read (from `ports/srclib.sh`):

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The GitHub repository holding the archive |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | The download base. A mirror laid out as `sources-NNN/<hash>` works unchanged. Empty skips the archive and fetches from upstream alone; `ports/publish` and the pre-push hook then refuse to run |
| `KDOS_SRCCACHE` | `ports/.srccache` | The local cache, one file per hash. Files already there need no network |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | Which archive release holds each hash. A hash it does not name is fetched from upstream |

---

## The orchestrator and the build tree

These failures come from how the build is run rather than from any one port: the package lists,
`kpkg`'s record of what is installed, the chroot's environment, and the order in which phases have
touched the build tree.

### A package list that does not resolve

```text
<phase>: package resolution failed - see build/logs/.../expansion.log
```

Before a package phase runs, the orchestrator asks `kpkgdepends` to turn the phase's
`packages.txt` into a build order. When that fails, nothing in the phase is built, and
`build/logs/<phase>/expansion.log` holds the solver's output: usually a port name that does not
exist, or a `depends` entry naming one. Preflight checks both (every package named in a
`packages.txt` has a port, every `depends` entry names a port, every list resolves to a dependency
order), so run it before re-running the build.

### The ISO is in use

```text
ERROR: build/iso-build/kdos.iso is open by another process — a running VM?
```

`make build` refuses to start while another process holds the ISO open, because rewriting it would
give that guest I/O errors on anything it has not already cached. Shut the virtual machine down.
`make build ALLOW_ISO_IN_USE=1` overrides the check.

### A recipe change that did not rebuild its port

`kpkg` prints `Skipping <port> (already installed)` for a port you have just changed.

`kpkg` decides whether an installed port is current by comparing a *recipe hash* with the one it
recorded at install time. For a port with a `source =` line, that hash covers `kpkgbuild`,
`build.sh`, `postinstall.sh` and every `.patch` in the port directory, and nothing else; the
tarballs are covered by their own `sha256 =` lines. For a port with no `source =`, which builds out
of its own directory as the ports under `src/` do, every file in the directory counts. Two cases
follow:

- A file beside a recipe that is none of those four kinds, such as a desktop entry or a
  configuration file the build installs, changes nothing `kpkg` sees. Bump `release =` in the same
  change, or rebuild the port by name.
- The hash is recorded *after* the install succeeds, from the port directory as it is at that
  moment. Editing a port's files while its rebuild is running records the edited recipe as built,
  and the next build skips it. Do not edit a port during its own rebuild.

To force a rebuild:

```sh
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"
```

### A variable that does not reach the chroot

A variable passed to `make build` changes the steps that run in the build container and has no
effect on any step that runs inside the chroot, including every packaging step.

`script/chroot_exec.sh` enters the chroot with `env -i`, which clears the environment, and names
the few variables that pass through (`HOME`, `TERM`, `PATH`, `KDOS_REPLAY`, `KDOS_ISO_SOURCES`,
`KDOS_PACK_KDOS`). An opt-in flag therefore needs two edits: the `Makefile` passes it into the
container with `-e`, and `script/chroot_exec.sh` names it on the `env -i` line. See
[`env -i` means every variable must be named](build-system.md#env--i-means-every-variable-must-be-named).

### A package manager older than its source

`kpkg` is not a port. `script/01_phase1/12_kpkg.sh` compiles it straight into the target tree and
records a hash of its sources in `build/mark/phase1/kpkg`. A run that includes phase one recompiles
it when that hash changes; a run that starts later, such as `--continue-from 04_phase4`, never does,
so every later install and upgrade uses the older binary in the target tree. The current `kpkg`
keeps a file another installed package still lists (`Keeping <path>: <package> claims it`); an
older one can remove it as an orphan instead. The tool is then missing for every port built after
it: an upgrade of toybox that takes `cmp`, for example, makes bzip2's test fail with
`make: cmp: No such file or directory`.

Recompile `kpkg` in the target tree in place before continuing. A plan that names a step suppresses
snapshots and sets `KDOS_REPLAY=1`, so the step's "already built" guard stands down and the
phase-one snapshot is not overwritten:

```sh
make build BUILD_ARGS="--phases 01_phase1 --steps 01_phase1:12_kpkg.sh"
```

Files an older `kpkg` has already removed come back only when their owning port reinstalls. That
needs a recipe change to the port, or a `--rebuild` of it.

### A tool missing after toybox reinstalls

A tool that some other port installs (`cmp`, `readelf`, `strings`, `gunzip`, `mount`, `insmod`) is
missing or broken after a phase that reinstalled toybox.

toybox's recipe compiles out almost every applet whose name another port on the image owns, so
that the real tool is the only one. It keeps `sed`, `find`, `xargs`, `awk`, `expr` and `ln`, which
every configure script before the GNU ports needs and which those ports install over later. A
toybox installed after a port that owns one of those names holds that name in its manifest, and a
toybox upgrade removes every name its previous manifest held, taking the other port's file with it.
`kpkg` skips a port whose recipe hash is current, wherever it is listed, so the owning port is not
reinstalled on its own. The comment under the `toybox` line in `script/04_phase4/packages.txt`
states the rule: a name toybox compiles out needs a `release =` bump to the port that owns it in the
same change, and those owners are listed straight after toybox in phase four so that they reinstall
before anything later needs them. bzip2's build runs `cmp`, which is why the order matters.

### Helpers compiled against the other C library

`ports/fetch`, `ports/publish` and `ports/update` compile helpers on first use and keep them in
`ports/.kpkgbin/`, `ports/.portup` and `ports/.portup-tools/`. `ports/.kpkgbin/kpkg` is recompiled
when any of its sources is newer than the binary, and `ports/.portup` when a `.c` file under
`src/tools/kdos-portup` is; `ports/.portup-tools/kpkg` is recompiled only when it is missing or
fails to execute. Run one of those tools inside a container that mounts the repository read-write,
such as an Alpine-based development container, and the helpers are left compiled against that
container's C library; a binary built against musl does not run under glibc, or the reverse, and the
failure does not say why. `ports/.portup-tools/kpkg` then fails to execute and is rebuilt on its
next use, but `ports/.kpkgbin/` and `ports/.portup` are kept as long as their sources are older.
Delete those two (from a container, if a container running as root left them owned by root) and let
the next run recompile them. `ports/.srccache` is plain data and does not need clearing.
[Where the build puts things](developing.md#where-the-build-puts-things) has the details.

### A snapshot the orchestrator refuses

After each phase the orchestrator archives the build tree to `build/snapshots/<phase>/` (see
[Snapshots](build-system.md#snapshots)). A snapshot it cannot take is reported as a notice, shown
in the build and kept in `build/logs/snapshots.log`, and that phase has no snapshot to restore or
continue from:

| Message | Cause and fix |
|---|---|
| `snapshot <phase> FAILED: only <size> free, need ~<size>` | The new archives are written beside the old ones, so the free space on `build/` must hold the phase's previous compressed snapshot plus a fifth (a third of the raw size for a first snapshot). A complete set has measured at roughly 84 GB. Free space, delete old snapshots from the startup picker (`D`), or run `make cleanbuild` (keeps `build/snapshots`) or `make clean` (removes them); both keep `build/keys` |
| `snapshot <phase> FAILED: build/ is mid-restore of <phase>; refusing to snapshot it` | `build/.restore-in-progress` exists: a restore was interrupted and the tree is part-extracted. A new build refuses to start in the same state with `a restore of <phase> never finished - build/ is inconsistent.` Restore a snapshot again, or run `make cleanbuild` |
| `snapshot <phase> FAILED: mounts still active under build/fs: <paths>` | Something is still mounted under `build/fs`. The orchestrator first unmounts leftover chroot mounts itself, lazily if it has to (`released stale chroot mount(s)`), and names at most three it could not release. Find what holds them, such as a shell or process still inside the chroot, stop it, and unmount the paths |

### Re-running an early phase on a later tree

Phase one run on a target tree that has already been through phase four works on files later
phases have replaced. Its scripts skip on their markers (see
[Guards in the early phases](build-system.md#guards-in-the-early-phases)), but one whose guard
stands down installs its phase-one build into `build/fs` over the newer versions, and a failure
that follows looks like an ordinary build error. A full run would also overwrite phase one's
snapshot with the later tree filed under phase one's name. Use `--continue-from` or a
narrowed plan instead; a plan that narrows the run suppresses snapshots unless `--snapshot` is
given. See [Snapshots](build-system.md#snapshots) and [Build plans](build-system.md#build-plans).

---

## Toolchains and language runtimes

KDOS is built on musl, with GCC as the system compiler (`CC=gcc` from phase three onward), LLVM
and Clang as ports, and Rust linked against the system LLVM. Most of the failures in this group
come from an upstream that assumes glibc, or a toolchain configured by guesswork.

### Rust with a binding generator

A Rust crate fails with `Dynamic loading not supported`, or its final link fails with undefined
references into a library's `.a` (`libcurl.a` and its `nghttp2_*`).

The musl target links statically unless told otherwise. A binding generator (bindgen) then cannot
load `libclang` at build time, and a crate that links a system library takes that library's
static archive without the libraries it depends on. Where the link succeeds, the binary carries a
private copy that no update to the library's port reaches. Every recipe under `ports/core` that
runs `cargo build`, `cargo install`, `cargo cbuild` or `cargo cinstall` sets the flag, and
preflight fails one that does not:

```sh
export RUSTFLAGS="-C target-feature=-crt-static"
```

A recipe whose crates run bindgen also points it at `libclang`, which saves it a search:

```sh
export LIBCLANG_PATH=/usr/lib
```

### An LLVM that guessed its triple

`fatal error: 'cstddef' file not found` from clang, or from bindgen through libclang, while gcc
compiles the same header.

`clang -print-target-triple` answers with the triple compiled into LLVM. When the `llvm` port does
not set one, LLVM guesses, and on this system the guess is `x86_64-unknown-linux-gnu`. Clang then
looks for a gcc installation under that triple, finds none beside gcc's `x86_64-pc-linux-musl`,
and searches no C++ header directory at all; it would also link against glibc's loader. The `llvm`
and `llvm21` ports pass gcc's own triple, `$(cc -dumpmachine)`, as `LLVM_HOST_TRIPLE` and
`LLVM_DEFAULT_TARGET_TRIPLE`. Clang and libclang compile the default in from LLVM's
`llvm-config.h`, so a changed LLVM triple needs `clang` rebuilt as well. A stale clang shows as
the wrong answer from `clang -print-target-triple` after `llvm` is fixed.

### A Rust release older than the system LLVM

`'+amx-tf32' is not a recognized feature for this target` from rustc, at the first
standard-library build.

The `rust` port links rustc against the system LLVM (`llvm-config = "/usr/bin/llvm-config"` in
the configuration its `build.sh` writes), not the copy in its own tarball. When the system LLVM is
a major version ahead of the one a Rust release was cut against, rustc can name something that
LLVM does not have: a target feature, a pass, an intrinsic. Upstream's fixes for the newer LLVM
land on rustc's development branch first, and the port carries those commits as patches beside
the recipe until a release contains them. The port carries two for LLVM 23:

| Patch | What it fixes |
|---|---|
| `rust-llvm23-amx-tf32.patch` | Drops the AMX-TF32 target feature and its intrinsics, which LLVM 23 does not have |
| `rust-llvm23-assign-guid.patch` | Adds the pass that assigns the global identifiers ThinLTO's summaries are keyed on, where rustc writes ThinLTO bitcode; LLVM 23 does not run it unless the pipeline asks |

Remove a patch from the recipe when the Rust version it targets includes the change; the patch
then fails to apply, and the build stops at that line.

### A crate newer than the toolchain

`rustc <version> is not supported by the following packages`.

Cargo refuses a crate whose declared minimum Rust version is higher than the toolchain, rather
than degrading. The repository pins the toolchain (the `rust` port), and the `kdos-fetch` container
installs the same version, read from the `rust` recipe, because a Cargo newer than the one that
will compile the port can write a lock file the target's Cargo refuses. The container reads its Go,
Node.js, GHC and cabal versions from their recipes for the same reason.

Pin the port to the newest release that builds. Bumping the toolchain instead means bumping the
`rust` port and regenerating every Rust vendor bundle against it, which touches every Rust port
rather than one. The declared minimum is not an oracle: it gates the refusal and says
nothing about what the code uses, so a release declaring an older minimum can still fail on a
language feature stabilised later. The only reliable test is compiling. See
[A Rust port's version is pinned by this tree's compiler](writing-ports.md#a-rust-ports-version-is-pinned-by-this-trees-compiler).

### A vendor bundle in the wrong place

`no matching package named '<crate>'` under the offline build, with the crate sitting in the vendor
directory the whole time.

Cargo finds its configuration by walking up from the current directory. A build that invokes it
from a subdirectory, or with an explicit manifest path from a parent, never reads a configuration
placed next to that manifest. Unpack the bundle where the tool will be standing. The recipe's
`vendordir` key says where the *vendoring* must run, which is beside the manifest; the two
directories are not always the same place.

The same message names a crate that is missing from the bundle when the build runs a second Cargo
manifest outside the workspace, such as an `xtask` crate that writes a manual page: it resolves
against its own lock file. List every such manifest in `vendorsync`, relative to `vendordir`, and
`ports/fetch` passes each to `cargo vendor --sync`. See
[Where the bundle goes](writing-ports.md#where-the-bundle-goes).

### A Python backend resolving a system tool

A build system is compiled from source inside a step that is supposed to be downloading.

`pip download --no-binary :all:` builds each source distribution's metadata while it downloads,
and a build backend that cannot find a system build tool on `PATH` resolves it as a package of the
same name from the index. The index's `cmake` package is CMake's entire source tree, compiled from
scratch; its `patchelf` is patchelf's source tree with an autotools build inside it.

Two fixes, and which applies depends on the tool:

- Give the fetch image what a metadata build needs, so the installer never reaches for the index's
  copy of a system tool. That is why `ports/Containerfile.fetch` carries `cmake`, `ninja-build`,
  `patchelf`, a compiler and the `python3-dev`, `libffi-dev` and `libssl-dev` headers.
- Name the backends as ports and disable build isolation, when they are packages KDOS should
  have anyway. Most Python recipes install with `--no-build-isolation`.

Where a recipe names an explicit closure with `pypackages`, `ports/fetch` vendors it with
`--no-deps`: letting pip resolve would drag in every dependency that is already a port and build
each one's metadata to find that out.

### A language standard reaching the compiler's own runtime

`unknown type name 'bool'` in a header under `gcc/config/`, while `libgcc` is being compiled.

Every phase's `CFLAGS` pins a C standard older than C23 (`-std=gnu99` in the toolchain phase and
phase one, `-std=gnu11` from phase two onward). GCC's configure copies `CFLAGS` into
`CFLAGS_FOR_TARGET`, the flags for the runtime libraries it builds with the compiler it has just
built. That runtime includes the target's own headers, which are written for the new compiler's
default standard and use `bool` without `<stdbool.h>`.

`script/01_phase1/10_gcc.sh` and the `gcc`, `gcc-arm-none-eabi`, `gcc-avr`,
`gcc-riscv64-unknown-elf` and `libstdcxx-arm-none-eabi` ports each pass `CFLAGS_FOR_TARGET` with
the `-std=` flag removed:

```sh
CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}"
```

### The installed C++ headers shadowing the ones being built

`'fenv_t' has not been declared in '::'`, followed by `Cannot compile std module`, in the
phase-one GCC log. Make carries on past it, so the step succeeds and the compiler ships with no
`import std;`.

Phase one builds the system compiler with the cross-compiler, whose own C++ headers are on its
default search path. A libstdc++ wrapper such as `fenv.h` reaches the C library's header with
`#include_next`, and the next directory holding that name is the cross-compiler's copy of the same
wrapper. Its include guard is already set, so the C library's declarations never arrive.

`script/01_phase1/10_gcc.sh` passes `CXXFLAGS_FOR_TARGET="$CXXFLAGS -nostdinc++"`, which leaves
only the headers of the libstdc++ being built. GCC's own configure adds `-nostdinc++` when it
builds the target libraries with the compiler in its own build directory; phase one builds them
with the cross-compiler instead, so the flag has to be passed.

### Gettext on musl

`undefined reference to libintl_gettext` (or `libintl_ngettext`) at link, from a program that
compiled without complaint.

musl has a `gettext()` in libc, but the GNU `libintl.h` the `gettext` port installs renames every
call to `libintl_gettext`, which only GNU's libintl defines; glibc provides those names and musl
does not. Upstream build systems routinely test `uname -s` and take Linux to mean glibc, so the one
platform that needs `-lintl` is the one their test excludes.

Depend on `libintl` and put `-lintl` where that project's link line will keep it. Check which
variable survives before choosing one: a `LIBS`-style variable set with `=` is clobbered by the
Makefile, while `LDFLAGS` is usually appended to with `+=` and sits last on the link line, which is
where a library reference belongs. Do not add `-liconv` alongside it out of habit: musl carries
iconv in libc and there is no library of that name.

`libintl` exists because the `gettext` port is built `--disable-nls` and installs the header
without the library, which is right for a system that ships no message catalogues. The `libintl`
port builds only the runtime library from the same tarball and version, so a caller's header and
library cannot describe two different gettexts.

### The wide curses API

`wget_wch`, `mvwaddnwstr` or another wide-character curses function reported as an implicit
declaration, against an ncurses that has it.

KDOS builds one ncurses, with wide-character support (`--enable-widec`), with its headers flat
in `/usr/include` and no `ncursesw/` directory. Two things follow. An include of
`<ncursesw/ncurses.h>` resolves nowhere. And `curses.h` declares the wide functions only when
`NCURSES_WIDECHAR` is 1, which it derives from `_XOPEN_SOURCE_EXTENDED`: without that define you
get the narrow API from the wide library.

Pass `-D_XOPEN_SOURCE_EXTENDED`, and for the include path build a directory of one symlink, added
with `-I`, as the `stfl` port does:

```sh
mkdir -p compat/ncursesw && ln -sf /usr/include/ncurses.h compat/ncursesw/ncurses.h
export CFLAGS="-I$PWD/compat -D_XOPEN_SOURCE_EXTENDED"
```

Both are build flags; neither edits the source. Pass them through the environment when the
Makefile appends its own with `+=`, because a command-line assignment replaces that variable
instead of extending it and takes `-fPIC` with it.

### An ICU component not propagated

Break-iterator symbols (`ubrk_*`) missing at link.

The break iterator lives in ICU's core library, `libicuuc`. In a shared build, ICU's
`icu-i18n.pc` lists `icu-uc` under `Requires.private`, so `pkg-config --libs icu-i18n` does not
emit `-licuuc`, and a program that asks only for the i18n component and calls the core directly
fails to link. Add the core library:

```sh
export LDFLAGS="$LDFLAGS -licuuc"
```

---

## The compact userland

KDOS's base userland is toybox, a single binary that provides most of the standard command-line
tools as applets. Its applets implement less than the GNU tools, and upstream build systems
routinely assume GNU behaviour. The GNU `gawk` (from phase two), `sed` and `findutils` (from
phase three) ports install over their applets, and `coreutils` installs exactly two programs,
`expr` and `ln`, because installing all of it would take about a hundred paths off toybox.

### A stream-editor extension that is not there

The build reaches the compiler and reports a missing type or an undefined constant. The real
problem is a generated header, script or configuration file that is zero bytes long.

toybox's `sed` implements several extensions (in-place editing with a backup suffix,
NUL-separated input), but not all of GNU's: the `0,/regex/` address form and the `\U` and `\L`
case-conversion escapes are among those it lacks, and a script using one can produce no output
rather than an error. A configure script that generates a header through a `0,/regex/d` script
gets an empty file from toybox's `sed`, and the build fails several steps later on an error naming
a type. GNU `sed` is built in phase three for that reason.

GNU `sed` is the `sed` port, listed in phases three and four. A port built in phase three before
it, or one that relies on the ordering, names `sed` in its `depends` line.

### Missing compact-userland features

A build step reports a subcommand or option that does not exist: an expression length operation,
or a relative-symlink option in an install script.

`expr length STRING` is undefined in POSIX, and toybox's `expr` yields an empty string for it. A
configure script that then compares the result numerically prints `integer expected` and falls
through to a misleading error; the `coreutils` recipe records brltty reporting a present speech
driver as unknown. toybox's `ln` has `-r` but not the long spelling `--relative`, which meson
install scripts use to make a symlink inside `DESTDIR` that stays correct once the tree is moved.
The `coreutils` port installs GNU `expr` and `ln` for these two reasons.

Each name added there replaces a toybox applet with a GNU program on every installed system,
while KDOS keeps toybox as its base userland. Prefer a build flag; add a name to
`INSTALL_PROGRAMS` in the `coreutils` recipe only when a toybox applet is missing a feature a port
cannot do without.

---

## Upstream build systems

The remaining failures come from the port's own build system meeting KDOS's constraints: no
network, musl, a newer compiler than upstream tested, and a parallel `make`.

### A build that reaches the network

`is the internet available?`, `cannot compute hash on failed download`, or `Could not resolve host`,
hours in.

The build container runs with `--network none`. Four shapes cause this:

| Shape | Looks like |
|---|---|
| A meson subproject fallback | A wrap being fetched |
| A CMake download call | A hash computed on a failed download |
| A dependency fetched from version control at configure time | A clone attempt |
| A Python build backend resolving a package | A package index request |

Which fix is right depends on what the thing being fetched actually is:

- A real library becomes a port. Look for the project's own escape hatch first; many have a "use
  the system copy" switch, and meson's `--wrap-mode=nodownload` stops a subproject being fetched.
- A header-only submodule nobody else uses becomes a second `source`, extracted where the probe
  already looks.
- A build-time generator becomes a host dependency.

CMake's `FetchContent` has a generic override: `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` forbids
downloads, and `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<dir>` points one dependency at a directory you
supply. The `libavif` port does this for `libargparse`, which it carries as a second source.

The `qemu` port is the worked example of the fourth shape. Its configuration builds a Python
environment and, with downloads enabled (its default), drops the offline flag, so the installer
goes to the network for packages vendored in the tarball itself and fails naming whichever it
asked for first. `--disable-download` is the flag. It also stops a device-tree submodule being
fetched, so a target that needs one then fails configuration; the port supplies the library
instead with `--enable-fdt=system` and a dependency on the `dtc` port.

### An unknown meson option

`Unknown options` at setup, before a line is compiled.

There is no universal spelling; one project's test-disable option is fatal in the next. Read the
option file (`meson_options.txt` or `meson.options`) out of the tarball, and use meson's built-in
options when you want a meson-level setting, because those are always valid. Preflight checks every
`-D` a recipe passes against the port's own option file, so the mistake is caught in the few
minutes preflight takes rather than when the port's turn comes in the build.

### A meson feature given a boolean

A configure-time error saying the value is not one of the choices, on a line that looks correct.

meson's *feature* type takes `enabled`, `disabled` or `auto`, and refuses a boolean. The *boolean*
type takes `true` and `false`. Both look alike in a recipe. Match the type in the option file.
Preflight validates these two types, the ones with a closed value set.

### A missing meson prefix or library directory

The build and install succeed, and at run time a shared library cannot be loaded
(`Error loading shared library`).

meson's default library directory is not on the runtime linker's search path. Pass
`--prefix=/usr --libdir=lib` on every `meson setup`.

A stale package-config file under the old prefix can also shadow the fixed one. After correcting a
prefix, delete the old files and the old package-config file, then rebuild the consumers.

### An old CMake policy floor

`Compatibility with CMake < 3.5 has been removed`, from a project whose `cmake_minimum_required`
predates CMake 3.5. The `cmake` port (4.x) refuses such a floor unless told otherwise:

```sh
-DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

### A misspelt CMake option

The option had no effect, and the build did the thing you disabled.

CMake prints a warning about manually-specified variables that were not used and carries on, the
opposite of meson, which fails at setup. Read that warning in the log rather than trusting the exit
status, and take option names from the project's own `option()` declarations.

### A CMake file with no project declaration

The library builds and installs, and then every static link against it fails on unwinder symbols.

A `CMakeLists.txt` with no `project()` gets an implicit one covering only C and C++, so assembly
sources are silently dropped and the archive is missing the routines written in assembly. The fix
is to configure from a directory whose `project()` enables the `ASM` language. LLVM's standalone
`libunwind/` directory has no `project()` of its own, so the `libunwind` port configures LLVM's
`runtimes/` directory (`project(Runtimes C CXX ASM)`) with `-D LLVM_ENABLE_RUNTIMES="libunwind"`.

### Go building into its own directory

The install step reports `Skipped dir` (toybox's `cp`) on a path that looks exactly
like the binary it wanted.

Building with an output name equal to an existing directory of the same name writes the binary
inside that directory. Build into a directory of your own and install from there.

### `C compiler cannot create executables`

Exactly that, from an old configuration script.

The message blames the toolchain and is almost never about the toolchain. Read the port's own
`config.log` (see [Reading a failure](#reading-a-failure)): the real error is next to the failing
test program.

For a configuration script generated by an old autoconf, the cause is the test program's old-style
C (implicit `int`, implicit declarations, unprototyped definitions), which GCC 14 and later treat as
errors; the `gcc` port builds GCC 16. Suppress the whole family at once, because each one otherwise
costs another round trip:

```sh
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
  -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
  -Wno-declaration-missing-parameter-type"
```

### A configuration script preferring another compiler

`C compiler cannot create executables` from a configuration script with a working compiler on the
search path the whole time.

The standard compiler-detection macro walks a preference list, and some projects put an
alternative compiler first. The moment that alternative becomes a port, every such recipe silently
changes toolchain. The environment of phase three and every later phase therefore sets `CC=gcc` by
name: a distribution that builds itself cannot have its toolchain depend on which ports happen to
be installed. A configuration script that ignores `CC` needs its own switch, passed in the recipe.

### Newer-compiler diagnostics as errors

`error: incompatible pointer types`, or a similar diagnostic that is a warning with older
compilers. GCC 14 made this one an error; disable it for the port:

```sh
export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"
```

### An upstream `-Werror`

The build fails on a warning you have never seen upstream report.

An upstream `-Werror` is a promise about upstream's compiler and C library, not about these. A
compiler newer than the one upstream tested, or musl's headers in place of glibc's, produces
warnings upstream has never seen. Turn `-Werror` off: `-Wno-error` in `CFLAGS`,
`--disable-werror` for a configure script that offers it, `-Dwerror=false` for meson. That keeps
every warning printed and stops upstream deciding which of them ends the build. Chasing them one
suppression at a time costs a round trip per diagnostic.

### Compiler flags passed as make arguments

An undeclared constant, or a size mismatch, that reads like a missing header.

A variable on the make command line overrides both the environment and the makefile's own
assignment, including its appending form. That is the wrong precedence for compiler flags, because
a makefile's own flags are its configuration: architecture width, installation paths, feature
constants. Export the flags instead of passing them as a make argument. A makefile that appends
then appends to yours, and one that assigns has already discarded the environment and needs
nothing from it.

### A backtick inside double quotes

`No rule to make target`, printed from the middle of an unrelated step.

An `echo` that tells somebody to run a command, written with backticks inside a double-quoted
string, does not print that instruction: the shell runs it. Quote a command a diagnostic names with
single quotes. Preflight checks every `echo` in the build system's own scripts under `script/`; in
a port's `build.sh`, the rule is yours to keep.

### A parallel install race

`No rule to make target '\'`, naming an object that compiled a moment earlier, during
`make install`.

Every phase exports `MAKEFLAGS=-j12`, so the install runs in parallel as well as the build. A
project whose install targets regenerate their own dependency files races itself: a half-written
`.dep` file ends on a bare line continuation, and make reads the backslash as a target.

Use `make -j1 … install` for that project, as the `libburn`, `libisofs`, `xfsprogs` and several
other ports do. A `-j` on the command line overrides the one in `MAKEFLAGS`, and only that
invocation is serialised; the compile keeps its parallelism. Being a race, it can pass on one
run and fail on the next, so a recipe that has built before is not evidence against it.

### A dependency the list happened to satisfy

A header that is plainly missing, in a port that has built many times before, on a fresh build
only. `sys/queue.h` is the one to expect on musl.

The port never declared the dependency and was satisfied by whatever else had already pulled it
in. In KDOS `sys/queue.h` (with `sys/tree.h` and `sys/cdefs.h`) comes from `libbsd`, which no
`packages.txt` names; it arrives as a dependency of ports such as `netcat`, `libtirpc` and `samba`.
An incremental build already has it. On an empty target tree the solver's order decides, and a port
built before its accidental provider fails.

Name the dependency in `depends`. The key exists so that the solver orders the build, and a
dependency that holds only by accident fails the first time the order changes.

### The time-protocol helper

GStreamer's precision-time-protocol (PTP) helper, `gst-ptp-helper`, is written in Rust, and
GStreamer's `meson.build` compiles it whenever meson finds a Rust compiler. The `gstreamer` port
does not depend on the `rust` port, so without a switch whether the helper is built, and whether
its Rust build can fail the port, would depend on what else the target tree happens to hold (see
[A dependency the list happened to satisfy](#a-dependency-the-list-happened-to-satisfy)). The port
disables it with `-Dptp-helper=disabled` on its `meson setup` line; a recipe building GStreamer's
core elsewhere needs the same option.

### An empty submodule in a release archive

A directory the build expects exists and is empty. A forge's generated release archive carries a
git submodule's directory empty, because the archive is made from the repository without recursing
into submodules. Add the submodule as a second `source` extracted where the build looks, or make
it a port.

### Source URLs and archive layouts

These rarely fail a build outright, but each can cost a fetch or an unpack round trip:

- A project's usual mirror can be stale while its real releases are elsewhere.
- Forge projects split between release assets and generated archives, at different URL shapes,
  and some forges' release download paths serve only files attached by hand; use the archive path
  instead.
- Some source hosts rate-limit archive generation, and some block automated fetching entirely, so
  a version cannot be checked from a script.
- The top-level directory in an archive varies. `kpkg` strips one path component from the first
  source, so an archive with no wrapping directory, or one whose members are written `./dir/…`,
  lands in the wrong place. Verify with `tar -tf` when the build reports the source is not where it
  should be; preflight checks both shapes.

## When the failure is not here

Work through these in order:

1. **The failing step's log**, under `build/logs/<phase>/`, read upward from the end.
2. **The port's own `config.log`**, if a configuration script failed, in its work directory
   under `build/fs/var/cache/kpkg/work/<name>/`. The failing test program is the error.
3. **`testing/preflight.sh`**, which catches wiring failures in a few minutes. Among other things
   it checks that every source a recipe declares is hashed (and lists the ones not fetched yet),
   that every source file in one of KDOS's own ports is compiled by its recipe, that every meson
   option exists, and that every dependency resolves: whole classes of failure that never reach a
   compiler.
4. **Whether the source was fetched at all.** The build has no network, so a declared source that
   `make fetch` did not put beside its recipe can only fail at the unpack. `make fetch-check` says
   which.
5. **Whether the target tree is ahead of the phase you are running.** See
   [Re-running an early phase on a later tree](#re-running-an-early-phase-on-a-later-tree).

## See also

- [Writing ports](writing-ports.md): the recipe format, the canonical build shapes, vendoring and
  publishing sources
- [The build system](build-system.md): phases, the chroot, step logs, snapshots and narrowed plans
- [Developing](developing.md): where sources come from, the make targets and the fast loops
- [Testing](testing.md): everything preflight checks before a build starts
- [How KDOS is built](how-kdos-is-built.md): the build from `git clone` to a bootable ISO, as one
  story
- [The ports catalogue](../06-reference/ports-catalogue.md): every port by phase and group, to find
  which phase builds the port that failed

<!-- book-nav -->
---

*Part V — Building and developing, chapter 34.* Previous: [33. Writing ports](writing-ports.md) · [Contents](../README.md) · Next: [35. The C libraries](c-libraries.md)

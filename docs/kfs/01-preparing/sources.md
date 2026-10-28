# Sources and the archive

A clone of the repository carries the recipes, the patches and KDOS's own code, but none of the
upstream source archives the recipes build from. This chapter shows how a recipe names its sources,
how `make fetch` puts every one of them in place and verifies it, where the copies are kept, and
how to check a tree offline before a build. `make fetch` is the only step that downloads sources.
`make build` uses the network only to build the `os-dev` image, the first time and whenever the
`Dockerfile` has changed (see [the image](host-container.md#the-image)), and the build itself runs with no network at all.

## How a recipe names a source

A port's `kpkgbuild` names its upstream files on `source =` lines, and gives the SHA-256 of each
file on a `sha256 =` line. The recipe of [pv](#port-pv), a small command-line tool, less its banner:

<!-- kfs:include ports/core/cli/pv/kpkgbuild -->

The `source =` line is a URL with the recipe's own `$name` and `$version` in it, which kpkg expands
when it reads the file; a recipe is parsed, never run as a script. The `sha256 =` line pairs a hash
with a file name, and the hash is what matters. Any copy of `pv-1.12.0.tar.gz` that hashes to
`31fdbdb4…` is the file the recipe means, wherever it came from, and a copy that does not is
refused, whatever its name says.

The first source of a recipe, when it is a URL ending in a recognised archive extension (`.tar.gz`,
`.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst` or `.zip`), is saved under the name
`<name>-<version>.<extension>`, whatever the rest of the URL says, so that a project that publishes
`v1.12.0.tar.gz` still arrives as `pv-1.12.0.tar.gz`. The `sha256 =` line names it that way. Any
other source keeps the last part of its URL as its name. A line that spells a name out as
`<file>::<url>` is saved under that name, first or not. The full rules for both keys are in
[Sources, and what each one becomes](../../kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes)
and [Checksums](../../kdos/05-developer/writing-ports.md#checksums).

## Fetching every source

From the top of the repository on your machine:

```bash
make fetch
```

This runs `ports/fetch` over every port. For each file a `sha256 =` line names that git does not
already carry, it stops at the first of these that holds a copy whose hash matches:

1. **The port directory**, when the file is already there from an earlier fetch.
2. **The local cache**, `ports/.srccache/sha256-XX/<hash>`, where `XX` is the first two hex digits
   of the hash.
3. **The KDOS [source archive]{.idx}**, the copy KDOS keeps of every file a recipe names, located through
   `ports/sources.idx`; described below.
4. **Upstream**, the URL on the recipe's `source =` line for that file.
5. **Generation**, for a port's own [vendor bundle]{.idx} only; described below.

A copy that fails its hash or is empty is deleted and the next place is tried. A download is
written to `<file>.part` and renamed into place only when it verifies, and an interrupted one is
resumed on the next run. The archive is asked before upstream because it does not disappear when an
upstream host does.

`ports/fetch` reads each recipe with kpkg's own parser, which it compiles on your machine from the
tree, so it wants a C compiler there (`$CC`, default `cc`). Without one, `make fetch` runs the
whole fetch in the `kdos-fetch` container described under [Vendor bundles](#vendor-bundles).

`make fetch FETCH_JOBS=4` works on four ports at once instead of one. Run `make fetch` again after
every pull or branch switch: it downloads only files no earlier fetch has placed, so on a tree
whose recipes have not changed it does nothing but check.

## The cache

Every copy that verifies is entered into `ports/.srccache/`, one file per hash, and the port
directory holds the same file. When the cache and the checkout are on one filesystem the two are
hard links to one copy and the cache costs no extra space. For `pv`:

```bash
stat -c '%i %h %s %n' ports/core/cli/pv/pv-1.12.0.tar.gz \
    ports/.srccache/sha256-31/31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8
```

```text
8428864 2 489837 ports/core/cli/pv/pv-1.12.0.tar.gz
8428864 2 489837 ports/.srccache/sha256-31/31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8
```

The two names have one inode number, the first column, and a link count of 2: they are one file.
The inode number is your filesystem's own and differs on every machine. Because the cache keys every file by its hash, a
branch switch that names files fetched before downloads nothing again. The cache is ignored by git
and survives both `make cleanbuild` and `make clean`; deleting it costs a full download and nothing
else. A second checkout can share it by setting `KDOS_SRCCACHE` to the same directory in both.

A fetched source is never committed. When a port moves to a new version, the next `make fetch`
removes the old tarball from its port directory: a file there that no line of the recipe names,
and that is the same inode as its cache entry, is deleted, and the copy in the cache stays. A file
beside a recipe changes the port's recipe hash unless a `sha256 =` line names it, so one left
behind would rebuild the port for bytes no build reads. A file that is not a link into the cache
(one copied across filesystems, or placed by hand) is left where it is; `testing/preflight.sh`
lists it.

## The archive

The KDOS source archive is a set of releases on the project's GitHub repository, numbered volumes
tagged `sources-1`, `sources-2` and so on, marked as pre-releases so that the repository's latest
release is always a system release. The files of each [shelf]{.idx} are kept in the volume that
the second field of the shelf's line in `ports/shelves` names, or in the lowest-numbered volume with
room when that one is full, and each file is stored under the asset name `<shelf>--<file>`, made longer when
that name is already taken. A file that several ports name is stored once, under the shelf of the
first port that names it, so a package page can show an archive copy whose name carries another
port's shelf and file name: the hash is what identifies it. Files are only ever added to the
archive, with two deliberate exceptions: `ports/publish --rehome` moves a file into its shelf's
volume, and `ports/publish --orphans --prune=yes-delete` deletes the files that neither a current
recipe nor the recipes of any released version (any `v*` tag) name. A checkout of any released
version can therefore still fetch the exact bytes its recipes were written against after upstream
has moved on.

`ports/sources.idx`, committed beside the recipes, says where each file is. Its header describes
the format:

<!-- kfs:include ports/sources.idx lines=1-17 pin=7c09ddeb67a1 -->

`pv`'s line, found by its hash:

```bash
grep ^31fdbdb4 ports/sources.idx
```

```text
31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8 sources-1 cli--pv-1.12.0.tar.gz pv/pv-1.12.0.tar.gz
```

so its archive copy is the asset `cli--pv-1.12.0.tar.gz` of the release `sources-1`, at
`https://github.com/kunaldawn/kdos/releases/download/sources-1/cli--pv-1.12.0.tar.gz`. A port's
package page lists this address beside the upstream one. The hash GitHub computes for an
asset is the recipe's hash, so the copy is verified the same way as one from upstream. The exact
addressing rules, including names GitHub rewrote and files too large for one asset, are in
[Where sources come from](../../kdos/05-developer/developing.md#where-sources-come-from).

## Vendor bundles

Software written in Rust, Go, Python, Haskell and Node usually downloads its own dependencies while
it builds, which a build with no network cannot allow. For such a port the dependencies are
collected in advance into a **vendor bundle**, `<name>-vendor-<version>.tar.xz`, a tarball made by
the language's own package manager and named on its own `sha256 =` line like any other source. A
recipe asks for one with a `vendoring =` key.

A bundle is normally in the archive and fetched like any other file. Only when none of the first
four places has it does `make fetch` generate it, in a second pass and in a separate container,
`kdos-fetch`, built from `ports/Containerfile.fetch` with the toolchain versions this tree's own
recipes name, so that the bundle it writes reproduces the recipe's hash. A clone whose archive is
complete never builds that container. See
[Vendoring](../../kdos/05-developer/writing-ports.md#vendoring).

## Checking a tree offline

Before a build, and after anything that could have touched the port directories, check that every
archived source is present and intact:

```bash
make fetch-check
```

This runs `ports/fetch --check` and uses no network, though it still compiles the recipe reader
and so needs the C compiler. It hashes every file a `sha256 =` line names that git does not carry,
prints one line for each that is absent or wrong,

```text
✗  pv/pv-1.12.0.tar.gz: missing
✗  pv/pv-1.12.0.tar.gz: does not match its sha256
```

and ends with a count, exiting 1 when there is any problem and 0 when there is none:

```text
==> All <N> archived sources present and verified
```

`ports/fetch --check pv` checks one port. A tree that passes builds with no network for as long as
its recipes stay the same.

## Publishing a new source

The other half of fetching belongs to whoever changes a recipe. A recipe whose `sha256 =` names a
file the archive does not hold builds on the machine that wrote it and nowhere else, so the new file
is uploaded with `ports/publish <port>`, which also writes its line into `ports/sources.idx`, and
that line is committed with the recipe. A `pre-push` hook in `script/hooks/` refuses a push whose
recipes name a hash the archive lacks, among its other checks of the ports layout, patch headers
and books. It is enabled once per clone:

```bash
git config core.hooksPath script/hooks
```

See [Publishing sources](../../kdos/05-developer/writing-ports.md#publishing-sources).

## See also

- [Fetching: the only step that uses the network](../../kdos/05-developer/how-kdos-is-built.md#fetching-the-only-step-that-uses-the-network): the same journey in the KDOS book
- [Where sources come from](../../kdos/05-developer/developing.md#where-sources-come-from): the archive's addressing, every variable `ports/fetch` reads, and the pre-push hook
- [Upstream archives are hash-checked release assets](../../kdos/01-philosophy/decisions.md#upstream-archives-are-hash-checked-release-assets): why the archive exists
- [Reading a package page](../00-front/reading-a-package-page.md#sources): how a page shows its sources

<!-- book-nav -->
---

*Part I. Preparing, chapter 2.* Previous: [1. The build host and the os-dev container](host-container.md) · [Contents](../README.md) · Next: [3. The fs/ overlay](fs-overlay.md)

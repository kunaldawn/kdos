# Build troubleshooting

This page is for anyone whose KDOS build, fetch or push has just failed: a contributor adding a
port, or someone building the system from source for the first time. Almost every failure here is
one of a few dozen recurring problems, and most of them report something other than what is
actually wrong. The page catalogues them by the symptom you will see.

Find your message in the symptom index below and follow the link. Each entry gives the message,
the cause, and the fix that works on this tree. If nothing matches, work through
[When the failure is not here](#when-the-failure-is-not-here) at the end.

Two things help before you start:

- The log of the failing step is under `build/logs/<phase>/`. The message at the very end of a
  build's output is usually not the error; read upward from the end of the log.
- `testing/preflight.sh` catches the wiring mistakes (a missing dependency, an unhashed source, an
  unknown meson option) in about two minutes, without a build. Run it after `make fetch`, because
  some of its checks read the fetched tarballs.

For the recipe format itself, see [Writing ports](writing-ports.md).

## Symptom index

| What you see | Section |
|---|---|
| `Dynamic loading not supported` from a Rust crate, or a static archive's undefined references at its link | [Rust with a binding generator](#rust-with-a-binding-generator) |
| `rustc-LLVM ERROR: '+<feature>' is not a recognized feature for this target` | [A Rust release older than the system LLVM](#a-rust-release-older-than-the-system-llvm) |
| `'cstddef' file not found` from clang or bindgen, in a header that compiles with gcc | [An LLVM that guessed its triple](#an-llvm-that-guessed-its-triple) |
| A missing type, from an empty generated header | [A stream-editor extension](#a-stream-editor-extension-that-is-not-there) |
| `length: not found`, or a relative-link option rejected | [Missing compact-userland features](#missing-compact-userland-features) |
| A package index reached during an offline build | [A build that reaches the network](#a-build-that-reaches-the-network) |
| `Unknown options: …` at meson setup | [An unknown meson option](#an-unknown-meson-option) |
| `Value 'false' for option … is not one of the choices` | [A meson feature given a boolean](#a-meson-feature-given-a-boolean) |
| `Skipped dir` on a path that looks like the binary | [Go building into its own directory](#go-building-into-its-own-directory) |
| `Compatibility with CMake < 3.5 has been removed` | [An old CMake policy floor](#an-old-cmake-policy-floor) |
| `rustc <version> is not supported by the following packages` | [A crate newer than the toolchain](#a-crate-newer-than-the-toolchain) |
| `no matching package named …` with the vendor tree present | [A vendor bundle in the wrong place](#a-vendor-bundle-in-the-wrong-place) |
| CMake being compiled from source during a download step | [A Python backend resolving a system tool](#a-python-backend-resolving-a-system-tool) |
| `No rule to make target` from inside a packaging step | [A backtick inside double quotes](#a-backtick-inside-double-quotes) |
| An option you passed had no effect, with a warning about unused variables | [A misspelt CMake option](#a-misspelt-cmake-option) |
| `error: incompatible pointer types` | [Newer-compiler diagnostics as errors](#newer-compiler-diagnostics-as-errors) |
| `unknown type name 'bool'` inside a GCC target header, while building libgcc | [A language standard reaching the compiler's own runtime](#a-language-standard-reaching-the-compilers-own-runtime) |
| `'fenv_t' has not been declared`, then `Cannot compile std module`, in phase one | [The installed C++ headers shadowing the ones being built](#the-installed-c-headers-shadowing-the-ones-being-built) |
| Warnings you have never seen upstream, made fatal | [An upstream `-Werror`](#an-upstream--werror) |
| An undeclared constant that reads like a missing header | [Compiler flags passed as make arguments](#compiler-flags-passed-as-make-arguments) |
| `C compiler cannot create executables` | [The configuration-script probe](#c-compiler-cannot-create-executables) |
| `Error loading shared library` at run time | [A missing meson prefix](#a-missing-meson-prefix-or-library-directory) |
| A stale package-config file shadowing a fixed one | [A missing meson prefix](#a-missing-meson-prefix-or-library-directory) |
| Every static link failing on unwinder symbols | [A CMake file with no project declaration](#a-cmake-file-with-no-project-declaration) |
| `ubrk_*` missing at link | [An ICU component not propagated](#an-icu-component-not-propagated) |
| `undefined reference to libintl_gettext` | [Gettext on musl](#gettext-on-musl) |
| `No rule to make target '\'` during an install | [A parallel install race](#a-parallel-install-race) |
| A BSD header missing on a fresh build only | [A dependency the list happened to satisfy](#a-dependency-the-list-happened-to-satisfy) |
| A wide-character curses function as an implicit declaration | [The wide curses API](#the-wide-curses-api) |
| A time-protocol helper failing to link | [The time-protocol helper](#the-time-protocol-helper) |
| A submodule directory present but empty | [An empty submodule in a release archive](#an-empty-submodule-in-a-release-archive) |
| A configuration script picking a different compiler | [A configuration script preferring another compiler](#a-configuration-script-preferring-another-compiler) |
| A downloaded archive that does not exist, or unpacks oddly | [URL and version landmines](#url-and-version-landmines) |
| `Removing orphan` for a file another package still lists, then `not found` on that tool | [A package manager older than its source](#a-package-manager-older-than-its-source) |
| `Source not found: …` at a port's unpack | [A source that was never fetched](#a-source-that-was-never-fetched) |
| `sha256 MISMATCH for the generated <name>-vendor-<version>.tar.xz` from `ports/fetch` | [A sha256 mismatch on a vendor bundle](#a-sha256-mismatch-on-a-vendor-bundle) |
| `sha256 MISMATCH for <file> from <url>` from `ports/fetch`, or `sha256 MISMATCH for <file>` from `kpkg` | [A downloaded source that fails its hash](#a-downloaded-source-that-fails-its-hash) |
| `No sha256 for <file> in the recipe — refusing to extract an unverified source` | [A source with no hash](#a-source-with-no-hash) |
| Preflight: `declares <file>, which is neither in the port directory nor hashed` | [A source with no hash](#a-source-with-no-hash) |
| Preflight: `ships <file>, whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or tar` | [An error page saved as a tarball](#an-error-page-saved-as-a-tarball) |
| Preflight: `recipe-hashed archives are tracked by git`, or `make fetch-check` reporting `All 0 archived sources` | [Sources still in the git index](#sources-still-in-the-git-index) |
| `cannot generate <name>-vendor-<version>.tar.xz without its source`, or `this needs docker or podman` | [A vendor bundle that cannot be generated](#a-vendor-bundle-that-cannot-be-generated) |
| `failed to build the recipe reader` from `ports/fetch` | [A vendor bundle that cannot be generated](#a-vendor-bundle-that-cannot-be-generated) |
| `pre-push: these sources are named by a recipe but not in the archive` | [Pre-push refused: an unpublished source](#pre-push-refused-an-unpublished-source) |
| `Failed to download` for every file, or `pre-push: cannot reach the source archive` | [Fetch cannot reach the archive](#fetch-cannot-reach-the-archive) |

---

## Rust with a binding generator

A Rust crate fails with `Dynamic loading not supported`, or its final link fails with undefined
references into a library's `.a` (`libcurl.a` and its `nghttp2_*`).

The musl target links statically unless told otherwise. A binding generator then cannot load the
compiler front-end library at build time, and a crate that links a system library takes that
library's static archive without the libraries it depends on. Where the link succeeds, the binary
carries a private copy that no update to the library's port reaches. Every recipe that runs cargo
exports the flag, and preflight fails one that does not:

```bash
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
```

## An LLVM that guessed its triple

`fatal error: 'cstddef' file not found` from clang, or from bindgen through libclang, while gcc
compiles the same header.

`clang -print-target-triple` answers with the triple compiled into LLVM. When the `llvm` port does
not set one, LLVM guesses, and on this system the guess is `x86_64-unknown-linux-gnu`. Clang then
looks for a gcc installation under that triple, finds none beside gcc's `x86_64-pc-linux-musl`, and
searches no C++ header directory at all. It would also link against glibc's loader. The `llvm` and
`llvm21` ports pass gcc's own triple, `$(cc -dumpmachine)`, as `LLVM_HOST_TRIPLE` and
`LLVM_DEFAULT_TARGET_TRIPLE`. Clang and libclang compile the default in from LLVM's `llvm-config.h`,
so a changed LLVM triple needs `clang` rebuilt as well, and a stale one shows as the wrong answer
from `clang -print-target-triple` after `llvm` is fixed.

## A Rust release older than the system LLVM

`rustc-LLVM ERROR: '+amx-tf32' is not a recognized feature for this target`, at the first
standard-library build.

The `rust` port links rustc against the system LLVM, not the copy in its own tarball. When the
system LLVM is a major version ahead of the one the release was cut against, rustc can name
something that LLVM has since removed: a target feature, a pass, an intrinsic. Upstream's fixes for
the newer LLVM land on rustc's development branch first. The port carries those commits as patches
beside the recipe, the same ones other distributions shipping that pairing carry, until a release
contains them. Drop each patch when the version it targets includes it; `patch` then refuses it as
already applied.

## A stream-editor extension that is not there

The build reaches the compiler and reports a missing type or an undefined constant. The real
problem is a generated header, script or configuration file that is zero bytes long.

KDOS's base userland is toybox, a single compact binary that provides most of the standard
command-line tools as applets (the *compact userland* below). Its `sed` is POSIX, and upstream
build systems routinely assume GNU `sed`'s extensions: a line-range delete from zero, case conversion, null-separated
input, in-place editing with a suffix. None of those fails loudly. The pipeline produces
nothing.

The tree ships GNU `sed` as the `sed` port, which installs over the toybox applet in a later
phase. A port that needs it names `sed` in its `depends` line.

## Missing compact-userland features

A build step reports a subcommand or option that does not exist — an expression length operation,
or a relative-symlink option in an install script.

toybox's applets are a subset of the GNU tools. `coreutils` installs exactly two
programs over them, `expr` and `ln`, because `expr length` is undefined in POSIX and unimplemented,
and `ln --relative` is what meson install scripts use to make a symlink inside `DESTDIR` that
stays correct once the tree is moved. GNU `sed`, `gawk` and `findutils` sit over their applets for
the same reason.

Growing that list takes another hundred paths off the compact userland, which is the opposite of
what this distribution is. Prefer a build flag.

## A build that reaches the network

`is the internet available?`, `cannot compute hash on failed download`, or `Could not resolve host`
— hours in.

The build runs with no network at all. Four shapes cause this:

| Shape | Looks like |
|---|---|
| A meson subproject fallback | A wrap being fetched |
| A CMake download call | A hash computed on a failed download |
| A dependency fetched from version control at configure time | A clone attempt |
| A Python build backend resolving a package | A package index request |

Which fix is right depends on what the thing being fetched actually is:

- A real library becomes a port. Look for the project's own escape hatch first; many have a "use
  the system copy" switch.
- A header-only submodule nobody else uses becomes a second `source`, extracted where the probe
  already looks.
- A build-time generator becomes a host dependency.

CMake also has a generic override for pointing a fetch at a directory you supply.

The `qemu` port is the worked example of the fourth shape. Its configuration builds a Python
environment and, with downloads enabled — its default — drops the offline flag, so the installer
goes to the network for packages vendored in the tarball itself and fails naming whichever it asked
for first. `--disable-download` is the flag, and it has a second consequence worth knowing before it
bites: it also stops a device-tree submodule being fetched, so some targets then fail configuration
on a missing dependency.

## An unknown meson option

`Unknown options` at setup, before a line is compiled.

There is no universal spelling; one project's test-disable option is fatal in the next.

Read the option file out of the tarball, and reach for meson's built-in options when you want a
meson-level knob, because those are always valid. `testing/preflight.sh` checks this for every
recipe, which turns an hour-long round trip into seconds.

## A meson feature given a boolean

A configure-time error saying the value is not one of the choices. It reads perfectly on the line.

meson's *feature* type takes `enabled`, `disabled` or `auto`, and refuses a boolean. The *boolean*
type takes `true` and `false`. Both look identical in a recipe.

Match the type in the option file. Preflight checks the two types with a closed value set.

## Go building into its own directory

The install step reports `Skipped dir` on a path that looks exactly like the binary it wanted.

Building with an output name equal to a subdirectory of the same name writes the binary inside that
directory. Build into a directory of your own and install from there.

## An old CMake policy floor

`Compatibility with CMake < 3.5 has been removed`.

```bash
-DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

## A crate newer than the toolchain

`rustc <version> is not supported by the following packages`.

Cargo refuses a crate whose declared minimum is higher than the toolchain, rather than degrading.
This tree pins the toolchain, and the fetch container pins the same one, because a package manager
newer than the one that will compile the port can write a lock file the target's refuses.

Pin the port to the newest release that builds. Bumping one means bumping the toolchain, the fetch
container and every vendored bundle together — a wave, not a version bump.

The declared minimum is not an oracle. It gates the refusal and says nothing about what the code
uses, so a release declaring an older minimum can still fail on a language feature stabilised
later. The only reliable test is compiling.

## A vendor bundle in the wrong place

`no matching package named '<crate>'` under an offline build, with the crate sitting in the vendor
directory the whole time.

Cargo finds its configuration by walking up from the current directory. A build that invokes it
from a subdirectory, or with an explicit manifest path from a parent, never reads a configuration
placed next to that manifest.

Unpack the bundle where the tool will be standing. Set `vendordir` in the recipe to say where the
*vendoring* must run, which is beside the manifest — the two directories are not always the same
place.

## A Python backend resolving a system tool

A build system is compiled from source inside a step that is supposed to be downloading.

The installer builds metadata while it downloads, and a backend that cannot find a system build
tool resolves it as a package of the same name from the index, dragging in that tool's entire
source tree.

Two fixes, and which applies depends on the tool:

- Name the backends as ports and disable build isolation, when they are packages this tree should
  have anyway.
- Give the fetch image what a metadata build needs, so the installer never reaches for the index's
  copy of a system tool. That is why the fetch container carries a build system, a generator,
  development headers and a compiler.

And vendor without resolving dependencies where the recipe names an explicit closure: letting the
tool resolve drags in every dependency that is already a port and builds each one's metadata to
find that out.

## A backtick inside double quotes

`No rule to make target`, printed from the middle of an unrelated step.

An echo that *tells* somebody to run a command, written with backticks inside a double-quoted
string, does not print that instruction. The shell runs it.

Quote a command a diagnostic names with single quotes. Preflight checks every `echo` in the build
system's own scripts under `script/`; in a port's `build.sh`, the rule is yours to keep.

## A misspelt CMake option

The option had no effect, and the build did the thing you disabled.

CMake prints a warning about manually-specified variables that were not used and carries on — the
opposite of meson, which fails at setup.

Read that warning in the log rather than the exit status, and take option names from the project's
own `option()` declarations.

## Newer-compiler diagnostics as errors

`error: incompatible pointer types`, or a similar diagnostic that was a warning elsewhere.

```bash
export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"
```

## A language standard reaching the compiler's own runtime

`unknown type name 'bool'` in a header under `gcc/config/`, while `libgcc` is being compiled.

Every phase's `CFLAGS` pins a C standard older than C23, and GCC's configure copies `CFLAGS` into
`CFLAGS_FOR_TARGET`, the flags for the runtime libraries it builds with the compiler it has just
built. That runtime includes the target's own headers, which are written for the new compiler's
default standard and use `bool` without `<stdbool.h>`.

The phase-one compiler, the `gcc` port and the bare-metal cross-compiler ports each pass `CFLAGS_FOR_TARGET` with the `-std=` flag removed:

```bash
CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}"
```

## The installed C++ headers shadowing the ones being built

`'fenv_t' has not been declared in '::'`, followed by `Cannot compile std module`, in the phase-one
GCC log. Make carries on past it, so the step succeeds and the compiler ships with no `import std;`.

Phase one builds the system compiler with the cross-compiler, whose own C++ headers are on its
default search path. A libstdc++ wrapper such as `fenv.h` reaches the C library's header with
`#include_next`, and the next directory holding that name is the cross-compiler's copy of the same
wrapper. Its include guard is already set, so the C library's declarations never arrive.

`script/01_phase1/10_gcc.sh` passes `CXXFLAGS_FOR_TARGET="$CXXFLAGS -nostdinc++"`, which leaves only
the headers of the libstdc++ being built. A compiler that builds its own runtime with the compiler
it has just built already does this.

## An upstream `-Werror`

The build fails on a warning you have never seen upstream report.

An upstream `-Werror` is a promise about upstream's compiler and C library, not about these. Two
fire here that upstream has never seen: a transposed-arguments warning on an allocation form that
is correct, and a warning inside a C-library header the code does not include directly.

Pass `-Wno-error`, or meson's equivalent. It keeps every warning printed and stops upstream
deciding which of them ends the build. Chasing them one suppression at a time is a round trip per
diagnostic.

## Compiler flags passed as make arguments

An undeclared constant, or a size mismatch, that reads like a missing header.

A variable on the make command line beats both the environment and the makefile's own assignment,
including its appending form. That is the wrong end of the precedence for compiler flags, because a
makefile's own flags are its configuration: architecture width, installation paths, feature
constants.

Export the flags; never pass them as a make argument. A makefile that appends then appends to
yours, and one that assigns has already discarded the environment and needs nothing.

## `C compiler cannot create executables`

Exactly that, from an old configuration script.

The message blames the toolchain and almost never is the toolchain. Read the port's own
`config.log` — the real error is on the failing test program.

For anything with a pre-modern configuration script it is the test program's function definition
style, which newer compilers promoted from warning to error. Suppress the whole family at once,
because each one otherwise costs another hour-long round trip:

```bash
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
  -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
  -Wno-declaration-missing-parameter-type"
```

## A missing meson prefix or library directory

The build and install succeed, and at run time a shared library cannot be loaded.

meson's default library directory is not on the runtime linker's search path. Pass `--prefix=/usr
--libdir=lib` on every meson setup.

The trap has a second edge: a stale package-config file under the old prefix shadows the fixed one.
After correcting a prefix, delete the old files *and* the old package-config file, then rebuild the
consumers.

## A CMake file with no project declaration

The library builds, installs, and then fails every static link on unwinder symbols.

A CMake file with no `project()` gets an implicit one covering only C and C++, so assembly sources
are silently dropped and the archive is missing the routines written in assembly.

The fix is a top-level include file enabling the assembly language, passed through CMake's
include-before-project option — which is included *by* the project declaration, the implicit one
included. One line, and nothing upstream is touched.

The parent build directory is not the answer: it pulls in a large module tree from a source package
many times the size, for a handful of files.

## An ICU component not propagated

Break-iterator symbols missing at link. The package-config file for one ICU component does not
propagate the core one.

```bash
export LDFLAGS="$LDFLAGS -licuuc"
```

## A parallel install race

`No rule to make target '\'`, naming an object that compiled a moment earlier, during `make
install`.

Every phase exports `MAKEFLAGS=-j12`, so the install runs parallel as well as the build. A project
whose install targets regenerate their own dependency files races itself: a half-written `.dep`
ends on a bare line continuation, and make reads the backslash as a target.

Use `make -j1 … install` for that project. A `-j` on the command line overrides the one in
`MAKEFLAGS`, and only that invocation is serialised — the compile keeps its parallelism.

Being a race, it passes as often as it fails, so a recipe that has built before is not evidence
against it.

## A dependency the list happened to satisfy

A header that is plainly missing — `sys/queue.h` is the one to expect on musl — in a port that has
built many times before, on a fresh build only.

The port never declared the dependency and was satisfied by whatever else had already pulled it in.
`sys/queue.h` comes from `libbsd`, which nothing lists directly; it arrives as a dependency of
other ports. Rebuild incrementally and it is already there. Run `make clean` first and the order
decides, so a port earlier in the list than its accidental provider fails.

Name it in `depends`. The point of the key is that the solver orders the build, and a dependency
that is only ever true by accident is one `make clean` away from not being.

## Gettext on musl

`undefined reference to libintl_gettext` (or `libintl_ngettext`) at link, from a program that
compiled without complaint.

glibc answers `gettext()` from libc and musl does not. Upstream build systems routinely test `uname
-s` and take Linux to mean glibc, so the one platform that needs `-lintl` is the one their test
excludes.

Depend on `libintl` and put `-lintl` where that project's link line will keep it. Check which
variable survives before choosing one: a `LIBS`-style variable set with `=` is clobbered by the
Makefile, while `LDFLAGS` is usually appended to with `+=` and sits last on the link line, which is
where a library reference belongs. Do not add `-liconv` alongside it out of habit — musl carries
iconv in libc and there is no library of that name.

`libintl` exists because the `gettext` port is built `--disable-nls` and installs the header without
the library, which is right for a system that ships no message catalogues. It is the same tarball
and version, so a caller's header and library cannot describe two different gettexts.

## The wide curses API

`wget_wch`, `mvwaddnwstr` or another wide-character curses function reported as an implicit
declaration, against an ncurses that certainly has it.

This tree builds one ncurses, widec, with its headers flat in `/usr/include` and no `ncursesw/`
directory. Two things follow. An include of `<ncursesw/ncurses.h>` resolves nowhere. And `curses.h`
declares the wide functions only when `NCURSES_WIDECHAR` is 1, which it derives from
`_XOPEN_SOURCE_EXTENDED` — without that define you get the narrow API from the wide library.

Pass `-D_XOPEN_SOURCE_EXTENDED`, and for the include path build a directory of one symlink:

```bash
mkdir -p compat/ncursesw && ln -sf /usr/include/ncurses.h compat/ncursesw/ncurses.h
```

added with `-I`. Both are build flags; neither edits the source. Pass them through the environment
when the Makefile appends its own with `+=`, because a command-line assignment replaces that
variable instead of extending it and takes `-fPIC` with it.

## The time-protocol helper

The `gstreamer` port's precision-time-protocol (PTP) helper fails to build. Disable that helper
with `-Dptp-helper=disabled` in the `meson setup` line.

## An empty submodule in a release archive

A directory the build expects exists and is empty. A release archive carries a git submodule's
directory empty, because the archive is generated from the repository without recursing.

Add the submodule as a second `source` extracted where the build looks, or make it a port.

## A configuration script preferring another compiler

`C compiler cannot create executables` from a configuration script with a working compiler on the
search path the whole time.

The standard compiler-detection macro walks a preference list, and some projects put an alternative
compiler first. The moment that alternative becomes a port, every such recipe silently changes
toolchain.

The environment of phase 3 and every later phase therefore sets `CC=gcc` by name. A distribution
that builds itself cannot have its toolchain depend on which ports happen to be installed. A
configuration script that ignores `CC` needs its own switch, passed in the recipe.

## URL and version landmines

Not failures so much as time sinks:

- A project's usual mirror can be stale while its real releases are elsewhere.
- Forge projects split between release assets and auto-generated archives, at different URL shapes.
- Some forges' release download paths need manually attached files; use the archive path instead.
- Some source hosts rate-limit archive generation.
- The top-level directory in an archive varies. Verify with a listing when the build reports the
  source is not where it should be.
- Some hosts block automated fetching entirely, so a version cannot be checked from a script.

## A package manager older than its source

`kpkg` is not a port. `script/01_phase1/12_kpkg.sh` compiles it straight into the tree and records a
hash of its sources in `build/mark/phase1/kpkg`. A run that includes phase one recompiles it when
that hash changes; a run that starts later, such as `--continue-from 04_phase4`, never does, so
every later install and upgrade uses the tree's older binary. An upgrade from an older `kpkg`
removes a file as an orphan even though another package still lists it. The tool is then missing
for every port built after it: toybox's upgrade takes `cmp`, and bzip2's test fails with
`make: cmp: No such file or directory`.

Recompile it on the tree in place before continuing. A plan that names the step suppresses
snapshots and sets `KDOS_REPLAY`, so the phase-one snapshot is not overwritten:

```sh
make build BUILD_ARGS="--phases 01_phase1 --steps 01_phase1:12_kpkg.sh"
```

Files an older `kpkg` has already removed come back only when their owning port reinstalls. That
needs a recipe change to the port, or a `--rebuild` of it.

## A source that was never fetched

```
Source not found: <file> (checked <port directory> and /var/cache/kpkg/sources)
```

The build has no network and reads sources only from the port directory and `kpkg`'s source
directory, `/var/cache/kpkg/sources` (`SOURCE_DIR`). Upstream archives are not in git: `make fetch` puts them in the port directories. A fresh
clone, a recipe edited since the last fetch, or a branch switch that changed a version all leave a
port without its archive. `make fetch-check` lists every archived source that is missing or fails
its hash, offline, in about half a minute (it hashes every source):

```sh
make fetch-check
make fetch                   # or: ports/fetch <port>
```

If the file is on disk under another name, the recipe names it differently from how it was saved.
The first `source` of a recipe is saved as `<name>-<version>.<ext>`, whatever the URL's own file
name is; see [Sources, and what each one becomes](writing-ports.md#sources-and-what-each-one-becomes).
Preflight reports such a file as one `no 'source =' line resolves to`.

## A downloaded source that fails its hash

```
✗  <port>: sha256 MISMATCH for <file> from <url>
   expected <recipe hash>
   got      <hash of what arrived>
```

`ports/fetch` deletes a download that does not match the recipe and tries the next location, so
this line alone is not a failure: the port fails only when every location gave the wrong bytes (a
resumed download that fails is first retried once from zero). `kpkg` prints
`sha256 MISMATCH for <file>` at build time when the file in the port directory has changed since it
was fetched.

When every location disagrees with the recipe, upstream has changed the file under the same name
(forges regenerate archives, and some projects re-roll a release). If the source was ever
published, the archive still holds the bytes the recipe names, so check the archive is reachable
(see [Fetch cannot reach the archive](#fetch-cannot-reach-the-archive)). If it never was, compare
the new file with upstream's announcement or signature before you trust it, then replace the
`sha256 =` line and [publish](writing-ports.md#publishing-sources) the file.

## A source with no hash

```
No sha256 for <file> in the recipe — refusing to extract an unverified source (KDOS_ALLOW_UNVERIFIED=1 to override)
```

or, from preflight, `declares <file>, which is neither in the port directory nor hashed`.

Every source a recipe names needs a `sha256 = <hash>  <file>` line, matched by file name. `kpkg`
refuses to extract a file with none, and `make fetch` can fetch such a file only from upstream,
never from the archive. The usual cause is a version bump made with `ports/update --no-fetch`, by
a group bump, or by hand: the `sha256 =` lines still name the old file. Fetch the new file and
record its hash:

```sh
ports/fetch <port>
sha256sum ports/core/<port>/<file>
```

`KDOS_ALLOW_UNVERIFIED=1` lets a build extract an unhashed source while you bring a port up.
Preflight fails any recipe that mentions the variable, so it cannot stay.

## An error page saved as a tarball

Preflight reports `ships <file>, whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or
tar`, or `ships <file> as an empty file`; without preflight, the build fails at
`tar: Error is not recoverable`.

A mirror that answers a download with an error page writes HTML under the tarball's name, and an
interrupted download can leave an empty file. A hash recorded from either verifies perfectly, since
it is the hash of those bytes. Delete the file, fetch it again from a working URL, and replace the
`sha256 =` line with the hash of the real archive.

## Sources still in the git index

Preflight fails with `<N> recipe-hashed archives are tracked by git — git rm --cached them`, and
`make fetch-check` reports `All 0 archived sources present and verified` without checking anything.

`ports/fetch` treats every file in the git index as carried by git, and skips it: it neither
fetches nor checks it. A source tarball still in the index, for example as a Git LFS pointer, is
therefore invisible to `make fetch` and `make fetch-check`, while `ports/publish` and the pre-push
hook treat an LFS pointer as a file to archive. Take the tarballs out of the index (the files on
disk stay where they are) and commit that change:

```sh
git rm --cached ports/core/<port>/<file>
```

Afterwards `make fetch-check` counts and verifies them, and `.gitignore` keeps them out of the
index.

## A vendor bundle that cannot be generated

`ports/fetch` generates a `<name>-vendor-<version>.tar.xz` only when no copy exists in the port
directory, the cache or the archive. Generating needs the language toolchains at this tree's
versions, so by default it runs in the `kdos-fetch` container. These messages mean it could not:

| Message | Cause and fix |
|---|---|
| `cannot generate <bundle> without its source` | The port's main tarball also failed to download, and a bundle is generated from it. Fix that download first |
| `ports/fetch: this needs docker or podman` | Neither container engine is on `PATH`. Install one, or set `KDOS_FETCH_HOST=1` to generate with this machine's own `cargo`, `go`, `pip` or `cabal`, which then have to be the versions the recipes name |
| `failed to build the recipe reader` | `ports/fetch` compiles `kpkg` on the host to read recipes, and there is no C compiler (`$CC`, default `cc`). A plain fetch hands the whole run to the container instead; `--check`, `--tree` and `KDOS_FETCH_HOST=1` need the host compiler |
| `--tree never generates` | `ports/fetch --tree <dir>` fetches for another checkout and does not generate bundles. Run that checkout's own `ports/fetch <port>` |

## A sha256 mismatch on a vendor bundle

```
✗  <port>: sha256 MISMATCH for the generated <name>-vendor-<version>.tar.xz
   expected <recipe hash>
   got      <hash of what was generated>
```

`ports/fetch` generates a vendor bundle only when neither the port directory, the cache nor the
source archive has one matching the recipe, and holds the generated bundle to the recipe's hash like any download. A
mismatch means the resolution differed from the one the hash records: a registry that moved, a
yanked crate, a toolchain version that changed the lock file. The generated file is deleted and
the port fails, because keeping it would put a bundle nothing verifies in front of the build.

If the bundle was ever published, it is in the archive and the fetch would not have generated it;
check `KDOS_SOURCES_BASE` is not empty and the archive is reachable. If it never was — the recipe
was written on a machine that did not publish — the recipe's hash names bytes that exist nowhere.
Replace the `sha256 =` line with the hash printed on the `got` line, build the port, and
`ports/publish <port>` so every other clone fetches that bundle instead of generating its own.

## Pre-push refused: an unpublished source

```
pre-push: these sources are named by a recipe but not in the archive:
  <port>/<file>  (<hash>)
pre-push: publish them first:  ports/publish <port>
```

The pushed commits name a source hash the `kunaldawn/kdos-sources` archive does not hold, so the
push would publish a recipe that builds only on this machine. Run the named command, which uploads
from the port directory or the cache, then push again. Uploading needs a token with write access to
the archive; the token, pacing and flags are in [`ports/publish`](writing-ports.md#portspublish),
and what the hook checks is in [The pre-push hook](writing-ports.md#the-pre-push-hook). Without that
access, push with the check bypassed and name the ports in your pull request, so a maintainer
publishes them: `KDOS_SKIP_PUBLISH_CHECK=1 git push …`.

## Fetch cannot reach the archive

`ports/fetch` asks the source archive before upstream, so an unreachable archive shows as every
archived source falling through to its upstream URL: each file prints a download line for the
archive and then one for upstream. That is slower, and fails outright for any file whose upstream
has gone, with `Failed to download <file>`. An archive that answers 404 for a file behaves the same
way for that file: either it was never published, or the archive repository is not publicly
readable. The pre-push hook refuses with
`cannot reach the source archive at <base> (curl exit N)`, or with
`source archive unreachable at <base> (HTTP N)` when the archive answers anything but 200 or 404,
rather than passing, since absence cannot be ruled out offline. `ports/publish` stops the same way.

Check the machine can reach `https://github.com` at all. Then use the variables `ports/fetch`,
`ports/publish` and the hook all read:

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos-sources` | The GitHub repository holding the archive |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | The download base. A mirror laid out as `sha256-XX/<hash>` works unchanged. Empty skips the archive and fetches from upstream alone; `ports/publish` and the pre-push hook then refuse to run |
| `KDOS_SRCCACHE` | `ports/.srccache` | The local cache, laid out like the archive. Files already there need no network |

## When the failure is not here

Work through these in order:

1. **The failing step's log**, under `build/logs/<phase>/`. The orchestrator prints its name.
2. **The port's own `config.log`**, if a configuration script failed. It is in the port's work
   directory, `/var/cache/kpkg/work/<name>/` inside the build tree (`build/fs`). The message at the
   end of the build output is usually not the error; the failing test program in `config.log` is.
3. **`testing/preflight.sh`**, which catches wiring failures in about two minutes. Among other
   things it checks that every source a recipe declares is hashed (and lists the ones not fetched
   yet), that every source file in one of KDOS's own ports is compiled by its recipe, that every
   meson option exists, and that every dependency resolves: four whole classes of failure that
   never reach a compiler.
4. **Whether the source was fetched at all.** The build has no network, so a declared source that
   `make fetch` did not put beside its recipe can only fail at the unpack. `make fetch-check` says
   which.
5. **Whether you are re-running an early phase on a later tree.** Re-running, say, phase 1 on a tree
   that has already been through phase 4 behaves unlike either, and the failure it produces looks
   like an ordinary build error. Use `--continue-from` or a narrowed plan instead (see
   [The build system](build-system.md)).

## See also

- [Writing ports](writing-ports.md) — the recipe format and the canonical build shapes
- [The build system](build-system.md) — phases, the chroot, and narrowing a rebuild
- [Developing](developing.md) — which log to read, and the fast loops
- [Testing](testing.md) — what preflight checks before a build starts

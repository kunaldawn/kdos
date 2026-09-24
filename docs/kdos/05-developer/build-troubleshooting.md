# Build troubleshooting

Almost every build failure on this tree is one of a few dozen recurring problems. This page
catalogues them by the symptom you will actually see, because most of them report something other
than what is wrong — which is exactly why they are worth writing down.

Start from the symptom index. Each entry gives the message, the cause, and the canonical fix.

## Symptom index

| What you see | Section |
|---|---|
| `Dynamic loading not supported` from a Rust crate | [Rust with a binding generator](#rust-with-a-binding-generator) |
| `rustc-LLVM ERROR: '+<feature>' is not a recognized feature for this target` | [A Rust release older than the system LLVM](#a-rust-release-older-than-the-system-llvm) |
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

---

## Rust with a binding generator

A Rust crate fails with `Dynamic loading not supported`.

Crates that generate bindings try to load the compiler front-end library dynamically at build time,
which a statically linked C library does not support.

```bash
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
```

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

The compact userland's stream editor is POSIX, and upstream build systems routinely assume the
full-featured one's extensions: a line-range delete from zero, case conversion, null-separated
input, in-place editing with a suffix. None of those fails loudly. The pipeline produces
nothing.

The tree ships the full-featured editor, which installs over the applet in a later phase. A port
that needs it names it in `depends`.

## Missing compact-userland features

A build step reports a subcommand or option that does not exist — an expression length operation,
or a relative-symlink option in an install script.

The compact userland's applets are a subset of the GNU tools. `coreutils` installs exactly two
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

A virtualisation port is the worked example of the fourth shape. Its configuration builds a Python
environment and, with downloads enabled — its default — drops the offline flag, so the installer
goes to the network for packages vendored in the tarball itself and fails naming whichever it asked
for first. Disabling downloads is the flag, and it has a second consequence worth knowing before it
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

Quote a command a diagnostic names with single quotes. Preflight checks every echo in the build
scripts.

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

A media framework's precision-time helper fails to link. Disable that helper.

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

Every native phase environment therefore sets the compiler by name. A distribution that builds
itself cannot have its toolchain depend on which ports happen to be installed.

## URL and version landmines

Not failures so much as time sinks:

- A project's usual mirror can be stale while its real releases are elsewhere.
- Forge projects split between release assets and auto-generated archives, at different URL shapes.
- Some forges' release download paths need manually attached files; use the archive path instead.
- Some source hosts rate-limit archive generation.
- The top-level directory in an archive varies. Verify with a listing when the build reports the
  source is not where it should be.
- Some hosts block automated fetching entirely, so a version cannot be checked from a script.

## When the failure is not here

Work through these in order:

1. The failing step's log, under `build/logs/<phase>/`.
2. The port's own `config.log` inside the work directory, if a configuration script failed. The
   message at the end of the build output is usually not the error.
3. `testing/preflight.sh`, which catches the dull wiring failures in seconds.
4. Whether the port ever compiled here at all. Preflight checks that every source a recipe
   declares is in the port directory, that every source file in one of our own ports is compiled
   by its recipe, that every meson option exists, and that every dependency resolves — four whole
   classes of failure that never reach a compiler. The build has no network, so a declared source
   that is not committed beside its recipe can only fail at the unpack.
5. Whether you are re-running an early phase on a later tree, which is a different problem wearing
   a build failure's clothes.

## See also

- [Writing ports](writing-ports.md) — the recipe format and the canonical build shapes
- [The build system](build-system.md) — phases, the chroot, and narrowing a rebuild
- [Developing](developing.md) — which log to read, and the fast loops
- [Testing](testing.md) — what preflight checks before a build starts

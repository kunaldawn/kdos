# Build troubleshooting

The failures that recur when building this tree, each with the symptom you will actually see, the
cause, and the canonical fix. Almost every build failure on KDOS is one of these.

**Find the symptom, not the cause.** Most of these report something other than what is wrong —
that is why they are worth a catalogue.

## Symptom index

| What you see | Section |
|---|---|
| `Dynamic loading not supported` from a Rust crate | [Rust with a binding generator](#rust-with-a-binding-generator) |
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
| Warnings you have never seen upstream, made fatal | [An upstream `-Werror`](#an-upstream--werror) |
| An undeclared constant that reads like a missing header | [Compiler flags passed as make arguments](#compiler-flags-passed-as-make-arguments) |
| `C compiler cannot create executables` | [The configuration-script probe](#c-compiler-cannot-create-executables) |
| `Error loading shared library` at run time | [A missing meson prefix](#a-missing-meson-prefix-or-library-directory) |
| A stale package-config file shadowing a fixed one | [A missing meson prefix](#a-missing-meson-prefix-or-library-directory) |
| Every static link failing on unwinder symbols | [A CMake file with no project declaration](#a-cmake-file-with-no-project-declaration) |
| `ubrk_*` missing at link | [An ICU component not propagated](#an-icu-component-not-propagated) |
| A time-protocol helper failing to link | [The time-protocol helper](#the-time-protocol-helper) |
| A submodule directory present but empty | [An empty submodule in a release archive](#an-empty-submodule-in-a-release-archive) |
| A configuration script picking a different compiler | [A configuration script preferring another compiler](#a-configuration-script-preferring-another-compiler) |
| A downloaded archive that does not exist, or unpacks oddly | [URL and version landmines](#url-and-version-landmines) |

---

## Rust with a binding generator

**Symptom:** a Rust crate fails with `Dynamic loading not supported`.

**Cause:** crates that generate bindings try to load the compiler front-end library dynamically at
build time, which a statically linked C library does not support.

**Fix:**

```bash
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
```

## A stream-editor extension that is not there

**Symptom:** the build reaches the compiler and reports a **missing type** or an undefined
constant. The real problem is a generated header, script or configuration file that is **zero bytes
long**.

**Cause:** the compact userland's stream editor is POSIX, and upstream build systems routinely
assume the full-featured one's extensions — a line-range delete from zero, case conversion,
null-separated input, in-place editing with a suffix. It does not fail loudly: the pipeline
produces **nothing**.

**Fix:** the tree ships the full-featured editor, which installs over the applet in a later phase.
A port that needs it names it in `depends`.

## Missing compact-userland features

**Symptom:** a build step reports a subcommand or option that does not exist — an expression length
operation, or a relative-symlink option in an install script.

**Cause:** the compact userland's applets are a subset.

**Fix:** the tree installs exactly **two** replacement applets over the compact ones for this
reason, listed in that port's recipe. Adding to that list takes another path off the compact
userland, which is the opposite of what this distribution is — so prefer a build flag.

## A build that reaches the network

**Symptom:** `is the internet available?`, `cannot compute hash on failed download`, or
`Could not resolve host` — hours in.

**Cause:** the build runs with no network at all. Four shapes cause this:

| Shape | Looks like |
|---|---|
| A meson subproject fallback | A wrap being fetched |
| A CMake download call | A hash computed on a failed download |
| A dependency fetched from version control at configure time | A clone attempt |
| A Python build backend resolving a package | A package index request |

**Fix — three answers, and which is right depends on what the thing is:**

- **A real library → a port.** Look for the project's own escape hatch first: many have a
  "use the system copy" switch.
- **A header-only submodule nobody else uses → a second `source`**, extracted where the probe
  already looks.
- **A build-time generator → a host dependency.**

There is also a generic override in CMake for pointing a fetch at a directory you supply.

**A virtualisation port is the worked example of the fourth shape**: its configuration builds a
Python environment and, with downloads enabled — its **default** — drops the offline flag, so the
installer goes to the network for packages vendored in the tarball itself and fails naming whichever
it asked for first. Disabling downloads is the flag, and it has a second consequence worth knowing
before it bites: it also stops a device-tree submodule being fetched, so some targets then fail
configuration on a missing dependency.

## An unknown meson option

**Symptom:** `Unknown options` at setup, **before a line is compiled**.

**Cause:** there is no universal spelling. One project's test-disable option is fatal in the next.

**Fix:** read the option file out of the tarball. Reach for meson's **built-in** options when you
want a meson-level knob — those are always valid. `testing/preflight.sh` checks this for every
recipe, which turns an hour-long round trip into seconds.

## A meson feature given a boolean

**Symptom:** a configure-time error saying the value is not one of the choices. It reads perfectly
on the line.

**Cause:** meson's *feature* type takes enabled, disabled or auto, and **refuses a boolean**. The
*boolean* type takes true and false. Both look identical in a recipe.

**Fix:** match the type in the option file. Preflight checks the two types with a closed value set.

## Go building into its own directory

**Symptom:** the install step reports `Skipped dir` on a path that looks exactly like the binary it
wanted.

**Cause:** building with an output name equal to a subdirectory of the same name writes the binary
**inside** that directory.

**Fix:** build into a directory of your own and install from there.

## An old CMake policy floor

**Symptom:** `Compatibility with CMake < 3.5 has been removed`.

**Fix:** `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

## A crate newer than the toolchain

**Symptom:** `rustc <version> is not supported by the following packages`.

**Cause:** the package manager **refuses** a crate whose declared minimum is higher than the
toolchain, rather than degrading. This tree pins the toolchain, and the fetch container pins the
same one — because a package manager newer than the one that will compile the port can write a lock
file the target's refuses.

**Fix:** pin the port to the newest release that builds, and say so in the recipe. Bumping one
means bumping the toolchain, the fetch container and every vendored bundle together — a wave, not a
version bump.

**The declared minimum is not an oracle.** It gates the refusal and says nothing about what the
code uses: a release declaring an older minimum can still fail on a language feature stabilised
later. The only reliable test is compiling.

## A vendor bundle in the wrong place

**Symptom:** `no matching package named '<crate>'` under an offline build, with the crate sitting
in the vendor directory the whole time.

**Cause:** the package manager finds its configuration by walking up from the **current
directory**. A build that invokes it from a subdirectory, or with an explicit manifest path from a
parent, never reads a configuration placed next to that manifest.

**Fix:** unpack the bundle where the tool will be standing. Set `vendordir` in the recipe to say
where the *vendoring* must run, which is beside the manifest — the two directories are not always
the same place.

## A Python backend resolving a system tool

**Symptom:** a build system is compiled **from source** inside a step that is supposed to be
downloading.

**Cause:** the installer builds metadata while it downloads, and a backend that cannot find a
system build tool resolves it as a **package of the same name** from the index — dragging in that
tool's entire source tree.

**Fix — two answers:**

- **Name the backends as ports and disable build isolation**, when they are packages this tree
  should have anyway.
- **Give the fetch image what a metadata build needs**, so the installer never reaches for the
  index's copy of a system tool. That is why the fetch container carries a build system, a
  generator, development headers and a compiler.

**And vendor without resolving dependencies** where the recipe names an explicit closure: letting
the tool resolve drags in every dependency that is already a port and builds each one's metadata to
find that out.

## A backtick inside double quotes

**Symptom:** `No rule to make target`, printed from the middle of an unrelated step.

**Cause:** an echo that *tells* somebody to run a command, written with backticks inside a
double-quoted string, does not print that instruction — **the shell runs it**.

**Fix:** quote a command a diagnostic names with **single** quotes. Preflight checks every echo in
the build scripts.

## A misspelt CMake option

**Symptom:** the option had no effect, and the build did the thing you disabled.

**Cause:** CMake prints a warning about manually-specified variables that were not used and
**carries on** — the opposite of meson, which fails at setup.

**Fix:** read that warning in the log rather than the exit status, and take option names from the
project's own option declarations.

## Newer-compiler diagnostics as errors

**Symptom:** `error: incompatible pointer types`, or a similar diagnostic that was a warning
elsewhere.

**Fix:**

```bash
export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"
```

## An upstream `-Werror`

**Symptom:** the build fails on a warning you have never seen upstream report.

**Cause:** an upstream `-Werror` is a promise about **upstream's** compiler and C library, not
about these. Two fire here that upstream has never seen: a transposed-arguments warning on an
allocation form that is correct, and a warning inside a C-library header the code does not include
directly.

**Fix:** `-Wno-error`, or meson's equivalent. It keeps every warning printed and stops upstream
deciding which of them ends the build. Chasing them one suppression at a time is a round trip per
diagnostic.

## Compiler flags passed as make arguments

**Symptom:** an undeclared constant, or a size mismatch, that reads as a missing header.

**Cause:** a variable on the make command line beats **both** the environment and the makefile's
own assignment — including its appending form. That is the wrong end of the precedence for compiler
flags, because a makefile's own flags are its **configuration**: architecture width, installation
paths, feature constants.

**Fix: export the flags, never pass them as a make argument.** A makefile that appends then appends
to yours, and one that assigns has already discarded the environment and needs nothing.

## `C compiler cannot create executables`

**Symptom:** exactly that, from an old configuration script.

**Cause:** it blames the toolchain and almost never is the toolchain. **Read the port's own
configuration log** — the real error is on the failing test program.

For anything with a pre-modern configuration script it is the test program's function definition
style, which newer compilers promoted from warning to error.

**Fix — suppress the whole family at once**, because each one otherwise costs another hour-long
round trip:

```bash
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
  -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
  -Wno-declaration-missing-parameter-type"
```

## A missing meson prefix or library directory

**Symptom:** the build and install succeed, and at run time a shared library cannot be loaded.

**Cause:** meson's default library directory is not on the runtime linker's search path.

**Fix:** `--prefix=/usr --libdir=lib` on **every** meson setup.

**The trap has a second edge:** a stale package-config file under the old prefix **shadows** the
fixed one. After correcting a prefix, delete the old files *and* the old package-config file, then
rebuild the consumers.

## A CMake file with no project declaration

**Symptom:** the library builds, installs, and then fails **every static link** on unwinder
symbols.

**Cause:** a CMake file with no project declaration gets an implicit one covering only C and C++ —
so assembly sources are **silently dropped**, and the archive is missing the routines that are
written in assembly.

**Fix:** a top-level include file enabling the assembly language, passed through CMake's
include-before-project option — which is included **by** the project declaration, the implicit one
included. One line, and nothing upstream is touched.

The parent build directory is **not** the answer: it pulls in a large module tree from a source
package many times the size, for a handful of files.

## An ICU component not propagated

**Symptom:** break-iterator symbols missing at link.

**Cause:** the package-config file for one ICU component does not propagate the core one.

**Fix:** `export LDFLAGS="$LDFLAGS -licuuc"`.

## The time-protocol helper

**Symptom:** a media framework's precision-time helper fails to link.

**Fix:** disable that helper.

## An empty submodule in a release archive

**Symptom:** a directory the build expects exists and is empty.

**Cause:** **a release archive carries a git submodule's directory empty.** The archive is generated
from the repository without recursing.

**Fix:** add the submodule as a second `source` extracted where the build looks, or make it a port.

## A configuration script preferring another compiler

**Symptom:** `C compiler cannot create executables` from a configuration script with a working
compiler on the search path the whole time.

**Cause:** the standard compiler-detection macro walks a **preference list**, and some projects put
an alternative compiler first. The moment that alternative becomes a port, every such recipe
silently changes toolchain.

**Fix:** every native phase environment sets the compiler **by name**. A distribution that builds
itself cannot have its toolchain depend on which ports happen to be installed.

## URL and version landmines

Not failures so much as time sinks:

- **A project's usual mirror can be stale** while its real releases are elsewhere.
- **Forge projects split between release assets and auto-generated archives**, at different URL
  shapes.
- **Some forges' release download paths need manually attached files**; use the archive path
  instead.
- **Some source hosts rate-limit archive generation.**
- **The top-level directory in an archive varies.** Verify with a listing when the build reports
  the source is not where it should be.
- **Some hosts block automated fetching entirely**, so a version cannot be checked from a script.

## When the failure is not here

In this order:

1. **The failing step's log**, under `build/logs/<phase>/`.
2. **The port's own configuration log** inside the work directory, if a configuration script
   failed. The message at the end of the build output is usually not the error.
3. **`testing/preflight.sh`**, which catches the dull wiring failures in seconds.
4. **Whether the port ever compiled here at all.** Preflight checks that every source file in our
   own ports is compiled by its recipe, that every meson option exists, and that every dependency
   resolves — three whole classes of failure that never reach a compiler.
5. **Whether you are re-running an early phase on a later tree**, which is a different problem
   wearing a build failure's clothes.

## See also

- [Writing ports](writing-ports.md) — the recipe format and the canonical build shapes
- [The build system](build-system.md) — phases, the chroot, and narrowing a rebuild
- [Developing](developing.md) — which log to read, and the fast loops
- [Testing](testing.md) — what preflight checks before a build starts

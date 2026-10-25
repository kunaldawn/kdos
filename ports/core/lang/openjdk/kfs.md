## About

OpenJDK is the Java Development Kit: the HotSpot JVM, `javac`, `jar`, `jlink` and the rest of the
tools, and the class library, installed as one JDK image under
`/usr/lib/jvm/java-<feature>-openjdk` with `/usr/lib/jvm/default-jvm` pointing at it and every
launcher linked into `/usr/bin`. It is the Java every JVM program in KDOS runs on (JOSM, the Swing
applications, the Java libraries built from their sources), with AWT built for X11, so a Java
desktop program runs under Xwayland.

It is compiled here from the OpenJDK source. Building it needs a JDK already, so the recipe's
second source is a bootstrap seed: Adoptium's musl build (Temurin for Alpine) of the same feature
release, used as the boot JDK and then discarded; nothing it built is in the package (see
[Bootstrap
seeds](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/why-kdos.md#bootstrap-seeds)).

## Flags

- `--with-boot-jdk="$_seedjdk"` — The unpacked seed. OpenJDK accepts a boot JDK of the same feature
  release or the one before it (`DEFAULT_ACCEPTABLE_BOOT_VERSIONS`), so a bump of `version` past a
  feature release bumps `_seed` and `_seedbuild` with it.
- `--with-extra-cflags`, `--with-extra-cxxflags`, `--with-extra-ldflags` — The tree's compiler and
  linker flags. configure ignores `CFLAGS`, `CXXFLAGS` and `LDFLAGS` from the environment, so
  `build.sh` saves them, unsets them and passes them in through these switches, which keeps the
  `-ffile-prefix-map` and build-id settings in the native code.
- `-D_LARGEFILE64_SOURCE` — Added to both C and C++ flags. musl declares the `*64` file functions
  the class library's native code calls only under this macro.
- `--with-jobs="$KDOS_JOBS"` — The build's parallelism. The makefiles take it from here and refuse
  a `-j` in `MAKEFLAGS`, which is why `build.sh` unsets `MAKEFLAGS` first.
- `--with-jvm-variants=server`, `--with-debug-level=release` — The C2-compiling server VM, in its
  release build; both are upstream's defaults, named here.
- `--with-native-debug-symbols=none` — Generates no debug symbols for the native code at all,
  neither inside the binaries nor as separate `.debuginfo` files; the default depends on the
  platform.
- `--disable-warnings-as-errors` — Lets a compiler warning stay a warning; the JDK's build treats
  warnings as errors by default. The warnings still show.
- `--disable-precompiled-headers` — Compiles HotSpot without precompiled headers.
- `--disable-ccache` — Does not compile through ccache.
- `--disable-headless-only` — Builds the full AWT, Java 2D and Swing for X11, not only the headless
  subset; Java desktop programs run under Xwayland (see [No Xorg server, and one
  carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
  The full build needs CUPS, ALSA, fontconfig and the X11 libraries, and configure refuses it
  without any of them; each is in `depends`.
- `--with-freetype=system`, `--with-harfbuzz=system`, `--with-zlib=system`,
  `--with-libjpeg=system`, `--with-giflib=system`, `--with-libpng=system`, `--with-lcms=system` —
  Links each of these from its port instead of compiling the copy that the OpenJDK source carries.
- `--with-jtreg=no`, `--with-gtest=no` — Looks for neither test framework; neither is a port.
- `--disable-dtrace` — Builds no DTrace probes.
- `--with-version-pre=` — An empty pre-release field. Without it configure puts `internal` there.
- `--with-version-opt=kdos` — Sets the optional field of the version string. Without it configure
  writes `adhoc.<user>.<directory>`, which names the build machine and makes the build
  unreproducible.
- `--with-version-build="$_build"`, `--with-vendor-name=KDOS` — The build number, a recipe helper,
  and the vendor name the JVM reports.
- `make jdk-image` — Builds the JDK image, `build/linux-x86_64-server-release/images/jdk`, which is
  copied into the package whole.

## Patches

- `posix-grep.patch` — Writes the match in `make/autoconf/basic_tools.m4` that decides whether `date` is GNU's as `grep -E "GNU|BusyBox|uutils"` instead of the basic-regular-expression alternation `"GNU\|BusyBox\|uutils"`, which toybox's `grep` does not implement. Without it configure takes `date` for BSD's and calls `date -j`, the build's ISO source date comes out empty, and `jar --date` stops the build. The [coreutils](#port-coreutils) port, in `depends`, supplies the GNU `date` the check is looking for.

## Notes

The build reads `SOURCE_DATE_EPOCH`, which must be set when configure runs, so the class files,
jmods and version strings carry the tree's pinned date.

[pandoc](#port-pandoc) renders the manual pages from their Markdown sources; without it the image
has none. `build.sh` moves them from the image's `man/` to `/usr/share/man/man1`, where `man`
looks, and deletes the image's copy.

The seed's binaries name Alpine's `libc.musl-x86_64.so.1` as their C library, which musl's loader
resolves to itself, so they run in the chroot unchanged.

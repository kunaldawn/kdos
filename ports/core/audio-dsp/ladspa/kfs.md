## About

The LADSPA SDK is the reference for the [LADSPA]{.idx} plug-in API, the plain C interface for
audio effects loaded from shared objects: it installs `ladspa.h`, which any host or plug-in
compiles against, five example plug-ins into `/usr/lib/ladspa`, and three commands —
`listplugins`, `analyseplugin` and `applyplugin`, which runs a chain of plug-ins over a sound file
through libsndfile. In KDOS, `rubberband`, `mlt`, `fluidsynth` and `hydrogen` build against it.

## Flags

- `CFLAGS="$CFLAGS $mk $LDFLAGS"`, `CXXFLAGS="$CXXFLAGS $mk $LDFLAGS"` — The SDK's makefile is
  a plain one with no `LDFLAGS`, and its link rules pass `$(CFLAGS)`. On the command line these
  replace the makefile's own `CFLAGS`, so `$mk` restates what it holds — `$(INCLUDES)`, `-Wall
  -Werror -fPIC` and `-DDEFAULT_LADSPA_PATH=$(INSTALL_PLUGINS_DIR)`, single-quoted so that make,
  not bash, expands them — around the tree's release flags, and the linker flags ride in the
  same variable to reach the link. Upstream's `-O2` is left to the tree's flags.
- `INSTALL_PLUGINS_DIR`, `INSTALL_INCLUDE_DIR`, `INSTALL_BINARY_DIR` — The makefile has no
  `DESTDIR`; the install step points its three install directories into `$PKG` instead.

## Patches

- `fix-memleak-in-plugin-scanning.patch` — Frees the path buffer in `search.c` for a directory entry that `dlopen` cannot load; without it, `listplugins`' directory scan leaks one buffer for every such file.

## Notes

- **Build and install must stay two `make` runs.** `DEFAULT_LADSPA_PATH`, the directory the tools
  search when `LADSPA_PATH` is unset, is compiled from `INSTALL_PLUGINS_DIR`. The first run builds
  with the makefile's value, `/usr/lib/ladspa/`; the second overrides it with the staging path
  only for the copy, and finds the binaries up to date. Building in the install run would compile
  the `$PKG` path into the programs.
- **`-Werror` stays on.** It is part of the makefile's `CFLAGS` that `$mk` restates, so a new
  compiler warning in this source fails the build.

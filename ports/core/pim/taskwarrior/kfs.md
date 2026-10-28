## About
Taskwarrior keeps a list of tasks, with due dates, priorities, projects, tags and dependencies,
and queries it from the command line with `task`. It is C++, but its storage layer is the Rust
crate `taskchampion`, bridged with `cxx`, so the port carries a
[Rust vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)
and CMake builds the Rust half through [corrosion](#port-corrosion).
[taskwarrior-tui](#port-taskwarrior-tui) is its interactive view.

## Flags
- `-DSYSTEM_CORROSION=ON` — Use the installed corrosion CMake package and fail if it is
  missing. Without it, `src/taskchampion-cpp/CMakeLists.txt` hands corrosion to `FetchContent`,
  which clones it from GitHub whenever `find_package` does not find it, and an offline build
  cannot make that fetch.
- `-DINSTALLED_CXXBRIDGE` — The `cxxbridge` program corrosion runs to generate the C++ side of the
  bridge. Corrosion uses an installed `cxxbridge` only when its version equals the `cxx` crate's;
  otherwise it runs `cargo install cxxbridge-cmd` from crates.io. `build.sh` therefore builds
  `cxxbridge-cmd` first, from the copy in the vendor bundle, with `cargo install --offline` and
  the workspace's `Cargo.lock`, so it is the same version, and passes its path here.
- `--config` — The two `--config` options of that `cargo install` point crates.io at the vendor
  directory. The copy of `cxxbridge-cmd` is built outside the source tree, where the bundle's
  `.cargo/config.toml` is not found by cargo's upward search.
- `LIBSQLITE3_SYS_USE_PKG_CONFIG=1` — Link `taskchampion`'s SQLite through pkg-config to the
  [sqlite](#port-sqlite) port rather than the copy `libsqlite3-sys` compiles; the chroot exports
  the same variable from `40_lang` on, and the recipe states it as well.
- `-DCMAKE_CXX_STANDARD_LIBRARIES=-lsqlite3` — Add `-lsqlite3` to the link of `task`. The Rust
  half is a static library that calls SQLite, and with the system library in its place the C++
  link has to name it.
- `-DENABLE_WASM=OFF` — Do not build for WebAssembly. It is the default.
- `-Wl,--strip-debug` — Have the linker leave debug sections out of every object it writes,
  whatever the objects it links carry. No port ships DWARF; see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh).

## Notes
Upstream's install puts the fish and zsh completions where those shells look, but the bash one
only among the documentation's scripts, so `build.sh` installs `scripts/bash/task.sh` as
`/usr/share/bash-completion/completions/task` itself.

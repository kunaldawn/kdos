## About

[The Powder Toy]{.idx} is a falling-sand physics sandbox: the player places particles of powders,
liquids, gases, explosives and electronic parts, and the game simulates air pressure, heat,
gravity and electricity between them. The program is `powder`; it saves simulations as local
`.cps` files and stamps, its reusable pieces of a simulation, as `.stm` files.

## Flags

- `-Dstatic=none` — link against the system's shared libraries. The other choices link statically,
  against system libraries or against the prebuilt bundle official builds download. Upstream's
  default, named.
- `-Dhttp=false` — build without libcurl. The online save browser, signing in, voting and the update
  check all talk to `powdertoy.co.uk` over HTTP; without it saves are opened and written as local
  files only, and [curl](#port-curl) is not in `depends`.
- `-Dignore_updates=true` — no notice of new versions. Upstream's default, named.
- `-Dcan_install=no` — keep the game from setting up its own file and URL associations. Left at
  `auto`, a release build offers at start-up to write a desktop entry and a MIME type into the
  user's home directory; this port installs both system-wide instead.
- `-Dlua=luajit` — the Lua scripting console and scripts run on [luajit](#port-luajit). `auto` picks
  by platform, which on Linux is LuaJIT too; naming it makes a missing LuaJIT a configure error.
- `-Dworkaround_elusive_bzip2=none` — find bzip2 through its pkg-config file like every other
  dependency. The default, `simple`, finds it by asking the compiler for `-lbz2` in its built-in
  directories.
- `-Duse_bluescreen=no` — no blue error screen with a stack trace on a crash. Its Linux stack-trace
  code uses `backtrace()` from `execinfo.h`, which musl does not have; `auto` turns it on for every
  build that is not a debug build.
- `-Dresolve_vcs_tag=no` — do not ask git for a version tag at build time, which upstream does
  through a target that is always out of date. Its default does so only for a static release
  build, so with `static=none` this names the outcome.
- `-Drender_icons_with_inkscape=disabled` — use the PNG icons the source ships in
  `resources/generated_icons` rather than rendering them from SVG with Inkscape. Upstream's default,
  named.

## Notes

`meson install` installs the program alone, so `build.sh` installs the rest from the source and
build trees: the manual page, two of the generated icons, and `build/resources/save.xml`, the
shared-MIME definition of `application/vnd.powdertoy.save`, which kpkg folds into the MIME database
(see [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). The
desktop entry is written by `build.sh` and claims that type, so a `.cps` or `.stm` file opens in
the game. Upstream's own entry also claims the `ptsave:` URL scheme, which opens the online save browser
that this build does not have, so the entry here leaves it out. The window's Wayland app id is
SDL's default, the executable's name, `powder`, which is what `StartupWMClass` names.

## About

libsass is the C/C++ implementation of the Sass stylesheet compiler. Upstream has deprecated it
in favour of Dart Sass, but it is what [sassc](#port-sassc) links, and sassc is the Sass compiler
that [breeze-gtk](#port-breeze-gtk) and [kde-gtk-config](#port-kde-gtk-config) run at build time
to turn Breeze's SCSS into GTK 3 and GTK 4 stylesheets. Nothing on KDOS uses it at run time.

## Flags

- `--disable-tests` — Configure without the test runner, which needs the `sass-spec` and
  `sassc` source trees beside this one.

## Notes

The GitHub archive carries no `configure` and no `VERSION` file, so `build.sh` runs
`autoreconf`. Autoconf reads the version from `version.sh` while it writes `configure`, and that
script takes `LIBSASS_VERSION` from the environment before it tries `git describe` and the
`VERSION` file; with neither in an archive, the version would be `[na]`, and `libsass.pc` and
`libsass_version()` would report that. `build.sh` passes the recipe's version in the variable, the
same result as Alpine's `fix-pkgconfig-version.patch`, without a patch.

## About

sassc is the command-line driver for [libsass](#port-libsass): it compiles a Sass or SCSS file to
CSS. It is a build tool: [breeze-gtk](#port-breeze-gtk) and [kde-gtk-config](#port-kde-gtk-config)
run it to build the Breeze GTK theme from its SCSS sources.

## Notes

The GitHub archive carries no `configure`, so `build.sh` runs `autoreconf` first, with
`SASSC_VERSION` set to the recipe's version. Autoconf reads the version from `version.sh`, which
takes that variable before it tries `git describe` and a `VERSION` file the archive does not have;
without it, `sassc --version` reports `[na]`.

Upstream ships no manual page. `build.sh` generates `sassc(1)` from the built program's `--help`
with [help2man](#port-help2man), as Alpine does; the page's date comes from
`SOURCE_DATE_EPOCH`, so it is the same on every build.

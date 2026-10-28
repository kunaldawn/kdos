## About

JS8Call is weak-signal keyboard-to-keyboard messaging for HF amateur radio: a conversational mode
built on the FT8 modulation, with directed messages, store-and-forward relays and a heartbeat
network. This port builds the JS8Call-improved continuation against Qt 6, and it keys the radio
through [hamlib](#port-hamlib).

## Flags

- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — Find Boost through the `BoostConfig.cmake` file that
  [boost](#port-boost) installs, instead of CMake's own FindBoost module, which policy CMP0167
  retires. The top-level `CMakeLists.txt`, the only one the build reads, already sets this
  policy to NEW whenever the running CMake knows it, so the preset repeats what the project
  asks for.
- `-DGIT_EXECUTABLE=GIT_EXECUTABLE-NOTFOUND` — Preset the cache entry that
  `find_program(GIT_EXECUTABLE git)` fills to a not-found value. The preset does not hide a
  `git` that is on `PATH`: `find_program` searches again for a variable whose value ends in
  `-NOTFOUND`. With no git found, the configure sets the build's revision string to `nogit`;
  with git found, it runs `git rev-parse --short HEAD` in the source directory, which fails
  because the release archive has no `.git`, and the revision string is left empty.

## Patches

- `no-update-check.patch` — In `JS8_UI/Configuration.cpp`, make the `CheckForUpdates` setting default to false. JS8Call otherwise asks GitHub for a newer release at every start; the check box on the settings page still turns it on.

## Notes

Upstream's CMake has no install rules (the changelog and copyright rules in `resources/debian`
belong to a directory the top-level `CMakeLists.txt` never adds), so `build.sh` runs no install
step: it installs the `JS8Call` binary by hand as lower-case `js8call`, the icon
PNG, two icon sizes rasterised from the SVG artwork, and a desktop entry. Qt's Wayland app id is
the executable's name when no organisation domain or desktop file name is set, so the entry's
`StartupWMClass` is `js8call`.

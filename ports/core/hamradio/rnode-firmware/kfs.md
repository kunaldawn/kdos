## About

rnode-firmware is the set of firmware images that RNode LoRa radios run, one per supported board,
as upstream publishes them for each release. Nothing in it runs on this machine: the images are
code for the radio's ESP32 or nRF52 processor, one of the exempt classes of
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
They are here so that `rnodeconf`, from [python3-rns](#port-python3-rns), can flash and update an
RNode with no network. For an ESP32 board it unpacks the board's archive and flashes it with the
`esptool.py` packed inside; it flashes an nRF52 by running `adafruit-nrfutil`, which is why
[python3-adafruit-nrfutil](#port-python3-adafruit-nrfutil) is a dependency.

## Notes

The images are installed under `/usr/share/rnode-firmware/<version>/`, under the names `rnodeconf`
asks for (each board's `rnode_firmware_<board>.zip`, and `console_image.bin`), beside the
release's `release.json`. The recipe saves every download under a name that carries the version,
and `build.sh` gives each file its upstream name back as it installs it. For each image it also
writes `<file>.version`, holding `<version> <sha256>` as read from `release.json`, which is the
record `rnodeconf` otherwise writes from the `release.json` it downloads, so `rnodeconf` checks
each image against the hash upstream published for it. `/usr/share/rnode-firmware/version` names
the version the package carries.

`rnodeconf` keeps its images in `~/.config/rnodeconf/update/<version>/` and downloads whatever is
missing there. `rnodeconf-local`, the script the port installs, links each packaged file into that
directory (links, not copies, so the cache follows the package when it is upgraded) and runs
`rnodeconf --nocheck --fw-version <version>` with the arguments it was given: `--nocheck` turns off
the online check for a newer release, and `--fw-version` names the packaged version, so flashing
and updating reach no network.

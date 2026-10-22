## About

[Skanlite]{.idx} is KDE's image-scanning program: it previews a page, lets an area be selected,
and saves the scan as an image. It drives the scanner through [libksane](#port-libksane) and
[ksanecore](#port-ksanecore) over SANE, so any scanner [sane-backends](#port-sane-backends) or
[sane-airscan](#port-sane-airscan) can reach, including a network scanner that speaks eSCL or WSD,
is one it can use.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build and install no interface translations. The packager
  removes every non-English catalogue and translated handbook in any case (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Require KDE's documentation tools, which
  upstream looks up as optional. With them the handbook is built and Help opens a local copy;
  without them it is silently left out.

## Notes

Upstream installs its icon as SVG only, which the panel's icon lookup does not read. `build.sh`
rasterises `sc-apps-org.kde.skanlite.svg` to PNG at 48, 64 and 128 pixels.

`build.sh` replaces upstream's desktop entry. `KAboutData` sets the desktop file name, so the
Wayland app id is `org.kde.skanlite`, not upstream's `skanlite`, and the new entry names it in
`StartupWMClass`.

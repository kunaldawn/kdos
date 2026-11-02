## About

plasma-workspace-wallpapers is the set of 38 wallpapers from past Plasma releases, each a
wallpaper package under `/usr/share/wallpapers/<Name>/` with a `metadata.json`, a screenshot and
its images at several resolutions and orientations, from which Plasma picks the one nearest the
screen. They appear in the desktop's wallpaper chooser beside the KDOS wallpapers and Breeze's
`Next`; none of them is a default.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The port has no message catalogues, so the switch changes
  nothing here; it is passed as in every KDE recipe, so a release that adds catalogues still
  builds none (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The source is about 308 MB and unpacks to about 309 MB, all of it images already compressed, so
the package is about as large as its source and is one of the larger packages in the tree for no
code at all.

The port is data only: the configure needs CMake and
[extra-cmake-modules](#port-extra-cmake-modules) for the install paths and nothing else, so its
closure reaches neither the graphics stack nor a toolkit and it is built in `41_system`. The
licences are per wallpaper, in each `metadata.json`: 20 are LGPL-3.0 and 18 CC-BY-SA-4.0.

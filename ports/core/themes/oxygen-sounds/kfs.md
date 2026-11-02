## About

Oxygen is the earlier KDE sound theme, kept as a choice beside [Ocean](#port-ocean-sound-theme)
in the System Sounds settings page: 63 stereo Ogg sound names under `/usr/share/sounds/oxygen`,
50 recordings and 13 links that reuse one of them for a second event, named by the freedesktop
sound naming specification, with an `index.theme` that KNotifications and libcanberra read like
any other sound theme.

## Flags

- `-D KF5_SUPPORT=OFF` — Install only the theme directory. Upstream's default also copies 50
  `Oxygen-*.ogg` files flat into `/usr/share/sounds`, the paths KDE Frameworks 5 applications
  name in their notification files. The tree has no KDE Frameworks 5, and no port's notification
  file names one of them.
- `-D KF_SKIP_PO_PROCESSING=ON` — The port has no message catalogues, so the switch changes
  nothing here; it is passed as in every KDE recipe, so a release that adds catalogues still
  builds none (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The port is data only: the configure needs CMake and
[extra-cmake-modules](#port-extra-cmake-modules) for the install paths and nothing else, so its
closure reaches neither the graphics stack nor a toolkit and it is built in `41_system`.

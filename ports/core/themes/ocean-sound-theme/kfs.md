## About

Ocean is Plasma's sound theme: 45 stereo Ogg sound names under `/usr/share/sounds/ocean`, 31
recordings and 14 links that reuse one of them for a second event, named by
the freedesktop sound naming specification, from `desktop-login` and `desktop-logout` through
`dialog-warning`, `device-added`, `battery-low`, `audio-volume-change`, `trash-empty` and
`message-new-email` to `alarm-clock-elapsed`, the alarm the Clock application plays.
KNotifications and libcanberra look an event's name up in the active theme. Its `index.theme`
names no `Inherits=`, so an event Ocean has no sound for is looked up in the `freedesktop` theme
from [sound-theme-freedesktop](#port-sound-theme-freedesktop), the fallback every lookup ends in.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — The port has no message catalogues, so the switch changes
  nothing here; it is passed as in every KDE recipe, so a release that adds catalogues still
  builds none (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The port is data only: the configure needs CMake and
[extra-cmake-modules](#port-extra-cmake-modules) for the install paths and nothing else, so its
closure reaches neither the graphics stack nor a toolkit and it is built in `41_system`. The
`Project Files` directory of the tarball holds the sound designer's sources and is not
installed. The `.license` file beside each recording is installed with it.

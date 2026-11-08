## About

plasma-pa is Plasma's sound control: the volume applet in the panel, the Sound page of System
Settings, the volume and mute keys, and the per-application and per-device sliders. It speaks
PulseAudio's protocol through [libpulse](#port-libpulse) and
[pulseaudio-qt](#port-pulseaudio-qt), which on KDOS is answered by `pipewire-pulse` from
[pipewire](#port-pipewire), started for each user by turnstile (see
[Audio](../../../../docs/kdos/03-architecture/session.md#audio)).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Build the Sound page's handbook, which requires
  [kdoctools](#port-kdoctools). It is upstream's default, named so the handbook cannot be dropped
  unnoticed.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_CanberraPulse=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_SoundThemeFreedesktop=ON`: make the two pieces of the click played
  when the volume changes requirements of the configure. The click goes through
  [libcanberra](#port-libcanberra)'s PulseAudio backend, `libcanberra-pulse.so` in
  `/usr/lib/libcanberra-<version>`, and plays the `audio-volume-change` sound of
  [sound-theme-freedesktop](#port-sound-theme-freedesktop). Upstream looks for both only to report
  them, and without either the sliders work and make no sound.

## Notes

[kirigami](#port-kirigami), [kirigami-addons](#port-kirigami-addons),
[kitemmodels](#port-kitemmodels), [kdeclarative](#port-kdeclarative) and [ksvg](#port-ksvg) are
QML modules the applet and the settings page load at run time.

The applet shows one stream per application and does not start the sound server: with no
`pipewire-pulse` answering on the user's runtime directory it shows no devices.

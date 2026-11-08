## About

KMix is KDE's sound mixer: a window of per-channel volume sliders for every card and stream, a
tray icon with a volume popup, and handling of the volume and mute keys through
[kglobalaccel](#port-kglobalaccel). On KDOS it is an alternative to the Plasma volume applet,
not a second one: it starts only when the person asks for it (see Patches).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_PulseAudio=ON` — The PulseAudio backend, which reaches
  PipeWire through its PulseAudio server and is the one that shows per-application streams.
  Upstream looks for [libpulse](#port-libpulse) as optional and drops the backend without an
  error; required, a missing library stops the configure.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_ALSA=ON` — The ALSA backend, for the hardware controls of each
  card, through [alsa-lib](#port-alsa-lib). Optional upstream, and dropped quietly without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Canberra=ON` — The click KMix plays when a volume changes, through
  [libcanberra](#port-libcanberra). Optional upstream; without it the volume feedback is not
  compiled.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

The patch changes two defaults that must move together. The autostart entry's condition reads
the `AutoStart` key and falls back to its own default when the key is absent, while the settings
dialog reads the same key with the default in `settings.kcfg`, and KConfig writes nothing when a
value equals its default. With only the entry changed, ticking "Start KMix on desktop startup"
would store nothing and KMix would still not start. `settings.kcfg` is compiled into the program
by `kconfig_compiler`, and upstream has no build option for either default. The patch is KDOS's
own; Alpine builds KMix with both defaults on.

## Notes

The OSS backend is compiled whenever the C library headers have `sys/soundcard.h`; with no OSS
device it finds no mixer and contributes nothing.

`restore_kmix_volumes.desktop` is a second autostart entry that runs `kmixctrl --restore` at a
Plasma login. It restores only volumes KMix itself saved, and returns at once for a card it has
never saved, so it changes nothing until KMix has been used.

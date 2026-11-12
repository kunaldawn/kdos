## About

Speech Dispatcher is the common speech interface of the desktop: Orca, KMouth, Dasher, Okular's
Read Aloud and every Qt TextToSpeech program talk to it, and it speaks with
[espeak-ng](#port-espeak-ng), its only synthesiser. Each user runs a server of their own, started on
the first client's connection. The package installs the server, its output module `sd_espeak-ng`,
the client library `libspeechd`, `spd-say`, the Python module `speechd` and `spd-conf`, which writes
a per-user configuration. See
[Speech and the sound card](../../../../docs/kdos/02-user-guide/accessibility.md#speech-and-the-sound-card).

## Flags
- `--with-espeak-ng` — build `sd_espeak-ng`, the one output module that speaks.
- `--without-espeak` — with `--without-flite`, `--without-pico`, `--without-ivona`,
  `--without-ibmtts`, `--without-voxin`, `--without-baratinoo` and `--without-kali`: no module for an
  engine this tree does not carry.
- `--with-alsa` — play through ALSA, whose default device is the session's PipeWire, and through no
  other audio interface: `--without-libao`, `--without-pulse`, `--without-pipewire`,
  `--without-nas` and `--without-oss`.
- `--disable-ltdl` — no libltdl; the output modules are separate programs and the server loads no
  plugins.
- `--without-systemdsystemunitdir` — with `--without-systemduserunitdir`: install no unit files,
  which KDOS has no service manager to read.
- `--enable-python` — install the `speechd` Python module and `spd-conf`, which imports it and finds
  its files through [python3-pyxdg](#port-python3-pyxdg).

## Notes

The build makes `sd_cicero`, `sd_dummy`, `sd_festival`, `sd_generic` and `sd_openjtalk` whatever
configure is told, and `sd_espeak-ng-mbrola` with `--with-espeak-ng`; it installs a module
configuration for every engine it knows. A `speechd.conf` with no `AddModule` line, as shipped, starts
every `sd_*` program it finds, so each of these would start beside `sd_espeak-ng` on every login and
fail for want of its engine, and `sd_dummy` would stay resident. `build.sh` deletes those programs
and every module configuration but `espeak-ng.conf` after the install, which leaves espeak-ng the
only module found. The server still adds `espeak-ng-fallback`, a second `sd_espeak-ng` with no
configuration, after the modules it found, and logs that it cannot load `sd_dummy`, its last
fallback; neither changes what speaks.

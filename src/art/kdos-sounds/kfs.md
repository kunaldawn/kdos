## About
`kdos-sounds` is the KDOS sound theme, `/usr/share/sounds/kdos/`. It holds the machine's own
voices — the login chime, the notification blip, the error buzz — under the freedesktop sound
names libcanberra looks up: `desktop-login`, `desktop-logout`, `message-new-instant`,
`dialog-error`, `dialog-warning`, `complete`, `device-added` and `device-removed`. Every other name
falls through `Inherits=ocean,freedesktop` to Plasma's Ocean theme and then to the freedesktop
one, so a KDE event KDOS has no voice for still sounds.

Nothing here is recorded. `build.sh` runs `kdos-sfx render`, which synthesises each voice with the
same square and triangle tones `kdos-sfx` plays at a console and encodes it to Ogg Vorbis through
`oggenc` from [vorbis-tools](#port-vorbis-tools); see
[kdos-sfx](../../../docs/kdos/04-programs/kdos-command.md#kdos-sfx). The theme is selected by
`[Sounds] Theme=kdos` in the distribution's kdeglobals, which KNotifications and the System Sounds
settings page both read.

## Notes
`index.theme` names `Example=desktop-login`, the sound the System Sounds page plays to preview the
theme. Without it the page asks for `theme-demo` first, which this theme does not carry, and the
inherited name answers with Ocean's demo, so the KDOS theme would preview as Ocean.

The voices are mono at 44.1 kHz and live in the `stereo` directory anyway: the directory is the
output profile a sound is written for, libcanberra plays a mono file through a stereo output, and
`stereo` is the profile it asks for first.

`render` encodes with a fixed Ogg serial number, so the same voices give the same files on every
build; an encoder left to pick its own serial writes a different file each time and the package
never reproduces.

`ocean-sound-theme` is in `depends` because it is what the inherited names resolve to; it is data
in 41_system, so naming it keeps this port's closure below the toolkits. This port is in
42_graphics because `kdos-sfx` is part of [kdos-tools](#port-kdos-tools), which is.

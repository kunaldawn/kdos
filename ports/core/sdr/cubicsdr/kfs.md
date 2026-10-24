## About

[CubicSDR]{.idx} is a software-defined radio receiver with an OpenGL spectrum and waterfall
display. It tunes any receiver [soapysdr](#port-soapysdr) has a module for, demodulates AM, FM,
SSB and the other analogue modes with [liquid-dsp](#port-liquid-dsp), and plays the audio. Its
interface is wxWidgets on GTK 3.

## Flags

- `-DUSE_HAMLIB=ON` — The radio-control panel, which tunes a transceiver through
  [hamlib](#port-hamlib). Upstream leaves it off.
- `-DUSE_SYSTEM_RTAUDIO=OFF` — Build the copy of RtAudio that CubicSDR carries. The tree's
  [rtaudio](#port-rtaudio) reports errors by return code, while CubicSDR
  catches the `RtAudioError` exception that older RtAudio releases throw.
- `-DUSE_AUDIO_PULSE=ON`, `-DUSE_AUDIO_JACK=OFF`, `-DUSE_AUDIO_ALSA=OFF`, `-DUSE_AUDIO_OSS=OFF` —
  Give that copy of RtAudio its PulseAudio backend alone, which pipewire-pulse answers on KDOS.
  These are upstream's defaults for the bundled copy.
- `-DENABLE_DIGITAL_LAB=OFF` — Leave out the "Digital Lab" testing features.
- `-DBUILD_DEB=OFF`, `-DBUNDLE_APP=OFF`, `-DCUSTOM_BUILD=OFF` — No Debian package or application
  bundle targets, and none of the custom-deploy optimisation and debugging options.

## Notes

Upstream's desktop entry names its icon by absolute path, which the panel does not resolve.
`build.sh` installs `src/CubicSDR.png` into the hicolor theme and writes an entry that names it as
`CubicSDR`. wxGTK leaves the program name as `argv[0]`, so the Wayland app id and the X11 class
are both `CubicSDR`, which the entry's `StartupWMClass` names.

`build.sh` ends by checking that `/usr/bin/CubicSDR` is executable and that its font directory,
`/usr/share/cubicsdr/fonts`, is in the package.

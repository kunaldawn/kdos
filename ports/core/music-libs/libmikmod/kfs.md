## About

libmikmod plays [tracker modules]{.idx}: MOD, S3M, XM, IT and the other formats of the tracker
programs, songs stored as patterns of notes plus the instrument samples. It is the music of
[kdos-bb](#port-kdos-bb), and cmus and mpd link it for module playback. The recipe builds it for
ALSA alone: on a bare console there is no PipeWire, and a desktop session's PipeWire is reached
through ALSA's `default` device anyway.

## Flags

- `--disable-alldrv` — Turn every output driver off (PulseAudio, OSS, ESD, NAS, OpenAL, SDL, sndio
  and the rest), so that the ones named after it are the only ones built.
- `--enable-alsa` — Build the ALSA driver.
- `--enable-wav` — With `--enable-raw`, `--enable-aiff`, `--enable-stdout` and `--enable-pipe`, turn
  back on the drivers that need no library and which `--disable-alldrv` also turns off: the WAV, raw
  and AIFF file writers and the standard-output and pipe outputs, which render a module to a file or
  hand it to another program.
- `--enable-threads` — Build the thread-safe library, whose calls take an internal lock. kdos-bb
  runs the mixer on a thread of its own and relies on that lock; see
  [The mixer runs on its own thread](../../../../docs/kdos/04-programs/kdos-bb.md#the-mixer-runs-on-its-own-thread).
- `--disable-dl` — Link `libasound` into the library instead of loading it with `dlopen` when the
  ALSA driver is probed, so `libmikmod.so` names `libasound.so.2` as a dependency the loader
  resolves when a program starts. Loaded with `dlopen`, a libasound that fails to load makes
  `ALSA_IsThere()` report the driver absent, and a program that lets libmikmod pick its driver, as
  kdos-bb does, falls through to the next one present: the WAV writer, which writes the music to
  `music.wav` in the working directory instead of playing it.

## Patches

- `alsa-nonblocking-update.patch` — leaves the ALSA stream in the non-blocking mode it is opened in, instead of switching it to blocking for playback, and makes `ALSA_Update()` hand over as many periods as the card accepts, at most eight (`ALSA_UPDATE_MAX_PERIODS`), and then return. `MikMod_Update()` is a pull interface called from the program's own loop; with a blocking stream each call waits out a whole period inside `snd_pcm_writei()`, so a program that calls it more often than real time sleeps there and its animation stops while the music plays.
- `alsa-null-close.patch` — closes the PCM handle on `ALSA_Init_internal()`'s failure path only when it is not null. When `snd_pcm_open()` fails the handle stays null, and alsa-lib's `snd_pcm_close()` asserts on a null handle; without the guard, a machine with no usable `default` device aborts the program inside `MikMod_Init()` instead of returning an error it can handle.

## Notes

The ALSA driver opens the PCM named `default` and takes no options, so it plays wherever that name
points: PipeWire in a desktop session, and nothing in a login with no session unless
`KDOS_ALSA_DEFAULT` names the card; see
[Audio on a bare console](../../../../docs/kdos/04-programs/kdos-bb.md#audio-on-a-bare-console).

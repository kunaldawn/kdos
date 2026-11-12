## About

eSpeak NG is the speech synthesiser of KDOS: Speech Dispatcher's one voice, and so Orca's, KMouth's
and every Qt TextToSpeech program's, and the voice BRLTTY links into its daemon for speech at the
text console. It covers over a hundred languages in about 15 MB. The package installs the `espeak-ng`
command, `libespeak-ng` with its eSpeak compatibility header, and the voice data. How speech reaches
the sound card is in
[Speech and the sound card](../../../../docs/kdos/02-user-guide/accessibility.md#speech-and-the-sound-card).

## Flags
- `--with-pcaudiolib=yes` — play audio through [pcaudiolib](#port-pcaudiolib), the library behind
  `espeak-ng "text"` and every program that asks the library to play rather than hand back samples,
  BRLTTY among them. Speech Dispatcher takes the samples and plays them itself.
- `--with-mbrola=no` — with `--with-sonic=no`: no MBROLA diphone voices, which are not a port, and
  no Sonic speed-up library, which is not one either.
- `ac_cv_header_valgrind_memcheck_h=no` — never compile in the Valgrind client requests, whether or
  not a Valgrind header happens to be installed.

## Notes

`close-audio-when-idle.patch` makes a program that speaks through the library hold the sound
device only while an utterance plays: it closes the device once an utterance drains or is
cancelled and reopens it at the same rate for the next, at the cost of a device open per
utterance. Without it the library opens the device on the first word and closes it only when the
program ends, so a BRLTTY with a speech driver would hold the sound card from its first word until
it stops, and PipeWire could not resume on it meanwhile. The patch is to be offered upstream.

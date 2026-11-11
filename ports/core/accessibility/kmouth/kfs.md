## About

KMouth speaks for people who cannot: a sentence is typed, or picked from a phrase book or the
history, and read aloud. It is a KDE application on Qt 6 that speaks through Qt TextToSpeech
([qt6-qtspeech](#port-qt6-qtspeech)), which this tree builds with its speech-dispatcher engine
only, so the voice comes from [speech-dispatcher](#port-speech-dispatcher) and its
[espeak-ng](#port-espeak-ng) module. Both are in `depends` for run time; the build does not look
for them.

## Flags

- `-D BUILD_DOC=ON` — Build the handbook and the `kmouth(1)` manual page through
  [kdoctools](#port-kdoctools), which is then a required package. On by default upstream; named so
  the handbook cannot drop out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Upstream sets `CMAKE_CXX_FLAGS_RELEASE` to `-Wall -Wno-switch -O2` itself, replacing CMake's
`-O3 -DNDEBUG`; a normal variable set in the project shadows the cache entry the command line
would set, so the port builds at upstream's level.

The phrase books for Catalan, Dutch, German and Swedish are data the program reads, not message
catalogues, and ship beside the English ones. The install also writes `/etc/xdg/kmouthrc`, the
toolbar layout upstream ships as the system default, and translated manual pages that kpkg's
language rule removes.

The voice KMouth offers on its first start is Qt TextToSpeech's choice for the session's locale: a
voice whose language matches the locale's. Under the system's `en_US.UTF-8` that is an English
voice. Under `C.UTF-8` Qt's locale names no language, and the voices that match it are the
eSpeak NG voices whose language Qt cannot parse, so the first of those, Bishnupriya Manipuri,
would be picked. The choice is saved in `~/.config/kmouthrc` and is changed in KMouth's
configuration.

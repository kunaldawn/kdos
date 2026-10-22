## About
Qt Speech is the QtTextToSpeech library for Qt 6: `QTextToSpeech` speaks text, and its engine
plugins do the speaking. This port builds the Speech Dispatcher engine, which speaks through
[speech-dispatcher](#port-speech-dispatcher) and so through whichever synthesiser it is set to,
and the `mock` engine, which upstream builds on every platform and which produces no sound.

## Flags
- `-DFEATURE_speechd=ON` — the Speech Dispatcher engine, named so that a missing
  `libspeechd` stops the configure.
- `-DFEATURE_flite=OFF` — no Flite engine; Flite is not a port.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).

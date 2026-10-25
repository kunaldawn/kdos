## About
Qt 5 Compat is the Core5Compat library and the Qt 5 graphical effects for Qt 6: the classes Qt 6
removed from QtCore, `QTextCodec`, `QRegExp`, `QStringRef`, `QLinkedList` and the rest, and
the `Qt5Compat.GraphicalEffects` QML module. A program ported from Qt 5 that still uses them
links it.

## Flags
- `-DFEATURE_textcodec=ON` — with `-DFEATURE_codecs=ON` and `-DFEATURE_big_codecs=ON`:
  `QTextCodec` and its codecs for non-Unicode text, each named. Because
  [qt6-qtbase](#port-qt6-qtbase) is built with ICU, the conversions go through
  [icu](#port-icu): the module compiles its ICU codec, and leaves out its own CJK tables
  (Big5, EUC-JP, EUC-KR, GB18030, JIS, Shift-JIS), which `big_codecs` builds only without ICU.
- `-DFEATURE_iconv=OFF` — no conversion through iconv. Upstream's condition for the feature is
  that ICU is not used, so with ICU the switch states what the configure would decide.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).

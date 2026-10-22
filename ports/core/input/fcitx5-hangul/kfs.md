## About

The Korean engine for [fcitx5](#port-fcitx5): Hangul input on the Korean keyboard layouts, and
Hanja conversion, through [libhangul](#port-libhangul). It is one of the language engines KDOS
builds for the input method; see
[Input methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods)
for how fcitx5 reaches applications. The build is upstream's CMake build with its tests off.

## Notes

The engine asks for fcitx5's `TestFrontend` module with a `REQUIRED` `find_package`, whether or
not its own tests are built, so it configures only against an fcitx5 built with
`ENABLE_TESTING_ADDONS=On`, which [fcitx5](#port-fcitx5) is. It also names the lowest fcitx5
version it builds against, so a new release of the engine can need a new fcitx5 first. It reads
libhangul's version from pkg-config and compiles against the newer libhangul interface when the
library is new enough, so a libhangul update can change the code this port compiles.

kpkg does not unpack a `.tar.zst`, so `build.sh` unpacks the source archive itself.

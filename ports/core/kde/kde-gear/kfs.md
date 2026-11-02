## About

`kde-gear` is a meta port: no source, a `build.sh` that installs nothing, and a `depends` line
naming the eight Gear metas — [kde-gear-core](#port-kde-gear-core),
[kde-gear-games](#port-kde-gear-games), [kde-gear-education](#port-kde-gear-education),
[kde-gear-pim](#port-kde-gear-pim), [kde-gear-multimedia](#port-kde-gear-multimedia),
[kde-gear-graphics](#port-kde-gear-graphics), [kde-gear-utilities](#port-kde-gear-utilities) and
[kde-gear-devtools](#port-kde-gear-devtools). `kpkg install kde-gear` brings every Gear application
in from the medium's package store; [kde-all](#port-kde-all) is this and
[kde-plasma](#port-kde-plasma) together.

## Notes

Gear is about 250 ports, and kpkg refuses a recipe whose `depends` names more than 128, so the
applications are split by kind into the eight metas this one names. Each Gear port that no other
port depends on is named by exactly one of them; preflight's Gear-meta check fails a Gear
application none names, which `kpkg install kde-gear` would leave out, and one two name. A Gear library
is named by none and arrives through the applications that link it. A Gear application another
port depends on, such as `dolphin`, `konsole` or `kaddressbook`, may still be named, and is, where
it belongs to the set a person would expect.

Every meta is in `50_desktop`, which is built after every phase the phase table checks, so naming a
`44_apps` application here leaves it a leaf of `44_apps`.

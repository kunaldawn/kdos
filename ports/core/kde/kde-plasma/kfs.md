## About

`kde-plasma` is a meta port: no source, a `build.sh` that installs nothing, and a `depends` line
that is the Plasma session. `kpkg install kde-plasma` on an installed machine brings the session in
from the medium's package store; on the image every port it names is already built by its phase.
The installation medium carries the session without this meta: `ports/base` names its list port by
port, leaving out `plasma-workspace-wallpapers`, `oxygen`, the Chinese, Japanese and Korean input
engines and `fcitx5-gtk`, which are groups (see
[The base image](../../../../docs/kdos/03-architecture/packaging.md#the-base-image)), so
`kpkg install kde-plasma` on an installed machine adds those.

## Notes

It names every Plasma module that is not a library, every port `50_desktop` builds that is not a
meta or an input-method engine's library, and the session's data and runtime pieces that live in
earlier phases because their closure puts them there: `breeze-gtk`, `breeze-icons`,
`ocean-sound-theme`, `oxygen-sounds`, `plasma-workspace-wallpapers`, `kdos-icons`,
`kdos-wallpapers`, `kdos-sounds`, `iio-sensor-proxy` and `fwupd-snapshot`. A library arrives
through the closure of what links it and is not named.

Two Plasma ports built in `43_toolkits` are named although that phase is the libraries': `oxygen`,
the opt-in widget style and decoration, which nothing links and which System Settings can offer
only when it is installed, and `krdp`, whose `krdpserver` and Remote Desktop page nothing else
pulls in.

`spectacle` and `plasma-sdk` are Plasma modules but are named by
[kde-gear-core](#port-kde-gear-core) and [kde-gear-devtools](#port-kde-gear-devtools) instead, so a
machine with the session and no applications has no screenshot tool and no Plasma SDK.

The ports named here are in `50_desktop` or earlier, and no `41`-`44` port names a session port:
a library such as [plasma-activities](#port-plasma-activities) that needs a session daemon at run
time leaves it to this meta.

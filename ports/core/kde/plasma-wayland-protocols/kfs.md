## About

Plasma Wayland Protocols is the set of protocol descriptions, in Wayland's XML, that KWin and the
Plasma programs speak beside the standard ones: the shell's panels and window management, output
devices and their management, screen casting, blur, contrast, slide, the decoration palette, DPMS,
fake input, the lock-screen overlay and others. It installs only the XML and a CMake package that
points at it; every consumer runs `wayland-scanner` or Qt's generator on the files itself.

## Notes

It has its own version and its own download directory, `stable/plasma-wayland-protocols/`, and is
not part of the Plasma release set; KWin 6.8.2 requires 1.23.0. `BUILD_TESTING` is on by
default here, and its tests would make the configure require `wayland-scanner` and the Wayland
client library, so `build.sh` turns it off. There is nothing to compile: `cmake --install`
copies the files straight from the configured tree. [qt6-qtbase](#port-qt6-qtbase) is in
`depends` all the same, because ECM's `KDEInstallDirs` asks Qt's `qtpaths` for its prefix and the
configure stops without it.

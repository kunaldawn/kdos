## About

Info Center is the read-only half of System Settings: About this System, and pages for the CPU,
memory, block devices, PCI and USB devices, graphics (OpenGL ES through EGL, Vulkan, VA-API,
OpenCL, Wayland, EDID), audio, network interfaces, firmware security, energy, sensors, interrupts
and Samba shares. `kinfocenter` is a link to [systemsettings](#port-systemsettings), which opens
with only these pages; the pages themselves are QML KCMs in
`/usr/lib/qt6/plugins/plasma/kcms/kinfocenter`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_DOC=ON` — Build the handbook, which requires [kdoctools](#port-kdoctools). It is
  upstream's default, named so the handbook cannot be dropped unnoticed.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_USB1=ON` — Make libusb a requirement. Upstream builds the USB
  devices page only when [libusb](#port-libusb) is found, and drops it otherwise with nothing but a
  line in the feature summary.

## Notes

Most pages show the output of a command-line tool. Each page's metadata names its tool in
`TryExec`, but System Settings does not read that key in a KCM's metadata, so a page whose tool is
missing is still listed and opens on an error; every tool a shipped page runs is therefore a
run-time dependency in `depends`: `aha` from [aha](#port-aha), which turns `fwupdmgr` and `ip`
output into HTML; `clinfo` from [clinfo](#port-clinfo); `dmidecode` from
[dmidecode](#port-dmidecode), which only the KAuth helper runs; `lspci` from
[pciutils](#port-pciutils); `sensors` from
[lm-sensors](#port-lm-sensors); `lscpu` and `lsblk` from [util-linux](#port-util-linux); `ip` from
[iproute2](#port-iproute2); `vulkaninfo` from [vulkan-tools](#port-vulkan-tools); `wayland-info`
from [wayland-utils](#port-wayland-utils); `eglinfo` from [mesa-demos](#port-mesa-demos); `vainfo`
from [libva-utils](#port-libva-utils); `di-edid-decode` from
[libdisplay-info](#port-libdisplay-info); `fwupdmgr` from [fwupd](#port-fwupd); `pactl` from
[libpulse](#port-libpulse); and `qdbus` from [qt6-qttools](#port-qt6-qttools), which the KWin
support information page runs from Qt's own binary directory, `/usr/lib/qt6/bin`, to ask KWin
for its report.

The X Server and GLX pages need `xdpyinfo` and `glxinfo`, neither of which the image ships:
[mesa-demos](#port-mesa-demos) is built without X11, and there is no X server outside Xwayland.
Upstream builds both pages with no option to leave them out, so `build.sh` deletes
`kcm_xserver.so` and `kcm_glx.so` after the install; kept, they would be listed under Graphics
and each would open on a message asking for the missing program.

The memory page and About this System read the firmware tables through a KAuth helper,
`kinfocenter-dmidecode-helper`, which runs `dmidecode` as root, found in `/usr/sbin` on the path
D-Bus activation gives it. Its two actions are granted by upstream's own policy through
[polkit](#port-polkit): `org.kde.kinfocenter.dmidecode.memoryinformation` to the active local
session and `org.kde.kinfocenter.dmidecode.systeminformation` to active and inactive ones, so
neither asks for a password in a session elogind tracks (see
[The security model](../../../../docs/kdos/03-architecture/security-model.md)).

Chimera's `fix-output.patch` is not carried: it works around a `wcout` locale fault in libc++,
and this tree builds C++ with GCC and libstdc++.

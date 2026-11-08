## About

libkscreen is the display configuration library of Plasma: it reads the outputs, their modes,
scale, position, rotation and priority from KWin over the `kde-output-device-v2` and
`kde-output-management-v2` protocols, and applies a new arrangement as one change. The Display
page of System Settings, the display OSD and PowerDevil use it. It also installs
`kscreen-doctor`, which prints the current arrangement and changes it from a shell
(`kscreen-doctor output.HDMI-A-1.scale.1.25`), and a library for DPMS.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the library's Qt translation catalogues, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QCH=OFF` — No API documentation in Qt Help format; no port builds QCH.

## Notes

The only backend built is KWin's Wayland one; the library has no X11 backend. Its zsh completion
for `kscreen-doctor` asks [jq](#port-jq) for the output names when it completes, and the configure
warns when jq is absent, so jq is in `depends` for run time.

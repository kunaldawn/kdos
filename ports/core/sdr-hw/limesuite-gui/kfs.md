## About

LimeSuiteGUI is the graphical utility for LimeSDR boards: it reads and writes the transceiver's
registers, loads and saves board configurations, updates firmware and gateware, and shows the
received spectrum. It is built from [limesuite](#port-limesuite)'s tarball, with the `LimeSuiteGUI`
target alone, and links the library that port installs.

## Notes

Upstream's desktop entry is installed as `LimeSuiteGUI.desktop`, not under its own name
`lime-suite.desktop`. The program is wxWidgets on GTK 3, whose window takes the program name,
`LimeSuiteGUI`, as its Wayland app id, and KWin finds a Wayland window's icon only in the entry
whose file name is that app id.

`build.sh` fails the build if a copy of `libLimeSuite.so` reaches the package, and checks that
the program needs the `libLimeSuite.so.23.11-1` the `limesuite` package installs.

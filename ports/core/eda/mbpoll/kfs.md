## About

mbpoll is a command-line Modbus master: it reads the coils, discrete inputs and input and holding
registers of a Modbus slave, and writes its coils and holding registers, over a serial line (RTU) or
over TCP, once or polling at an interval. It talks to a PLC, a power meter or a sensor without a
program being written for it, and it is built on [libmodbus](#port-libmodbus).

## Flags

- `-DMBPOLL_CUSTOM_RTS_GPIO=OFF` — On by default. With it on and libpiduino found (the option's
  help text says ARM only), mbpoll drives an RS-485 transceiver's direction pin through a GPIO
  line, and the install marks the program setuid root. The tree has no libpiduino port; the
  switch keeps a later one from turning either on.
- `-DMBPOLL_VERSION_OVERRIDE=$version` — The variable `version-override.patch` adds. It gives the
  version mbpoll reports, which upstream otherwise takes from `git describe --tags`.

## Patches

- `version-override.patch` — In `cmake/GitVersion.cmake`, makes `GetGitVersion()` use `v${MBPOLL_VERSION_OVERRIDE}` when that variable is set, before it tries git. The release tarball is not a git checkout, so `git describe` fails and the function falls back to `v1.0-0`: without the patch the program reports itself as version 1.0.

## About
The Python client for [Meshtastic]{.idx}, the LoRa mesh network: the `meshtastic` command and
library, which talk to a Meshtastic radio over USB serial, TCP or Bluetooth LE to send and
receive text and position messages and to configure the node. A LoRa mesh needs no licence and
no infrastructure. The radio's serial adapter is reached through the `dialout` group; see
[The device groups and the udev rules](../../../../docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).
[contact](#port-contact) depends on it.

## Flags
- `SKIP_CYTHON=1` — Read by dbus-fast's `build_ext.py`: build no Cython extensions, and
  install dbus-fast as pure Python. dbus-fast's `pyproject.toml` requires `Cython>=3,<3.3.0`,
  which the [python3-cython](#port-python3-cython) port does not satisfy. Unset, whether the
  extensions are attempted depends on whether Cython is importable in the build root.

## Notes
The bundle's `pypackages` are the runtime dependencies that are not ports: pypubsub, bleak,
dbus-fast and tabulate, installed by name with `--no-deps`, and protobuf, which is named in
`pypackages` but installed from the [python3-protobuf](#port-python3-protobuf) port; requests,
urllib3, idna, charset-normalizer, certifi, PyYAML, pyserial and packaging are ports in
`depends` as well.
A second copy of a module that a port also installs would be a path two packages own; see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

bleak, the Bluetooth LE library, is installed even for a radio on a serial port, because
`meshtastic/__main__.py` imports `BLEInterface` at the top, and without bleak the command fails
on that import before it reads its arguments. `pypackages` pins bleak to a release whose sdist builds
with poetry-core, a port. bleak's Linux backend is dbus-fast.

Build isolation is off and every backend is a port (setuptools, setuptools-scm, poetry-core).
pypubsub declares `setuptools>=68,<77`, which the setuptools port does not satisfy; without
isolation pip does not check the declaration.

After the install, `build.sh` removes two things. `site-packages/contrib` is a top-level
package of pypubsub's, its examples `netpubsub.py` and `wx_monitor.py`, under a generic name.
`/usr/bin/mesh-analysis` is the entry point of meshtastic's `analysis` extra, three of whose
four dependencies (dash, dash-bootstrap-components, pandas-stubs) have no port; the fourth is
[pandas](#port-pandas).

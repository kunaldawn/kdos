## About
[esptool]{.idx} talks to the ROM bootloader of Espressif's ESP8266 and ESP32 chips over a serial
line: it reads chip information, and writes, reads and erases flash. The bootloader is in mask
ROM, so a chip with nothing valid in flash still answers it. The package also installs
`espefuse` and `espsecure`, for the chips' eFuses and for flash encryption and secure-boot
signing. [meshtastic-firmware](#port-meshtastic-firmware) depends on it. The serial adapter is
reached through the `dialout` group; see
[The device groups and the udev rules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).

## Flags
- `BUILD_EXTENSION=yes` — Read by websockets' `setup.py`: build its C `speedups` extension and
  stop if it does not compile. Unset, the extension is optional, and a failed compile leaves
  pure-Python websockets with only a warning in the build log.

## Notes
The `pip install` line installs, with `--no-deps`, esptool itself and the seven distributions of
its runtime closure that are not ports: bitarray, bitstring, esp-pylib, intelhex, reedsolo,
rich-click and websockets, all from the vendor bundle that `pypackages` describes. pyserial,
cryptography, PyYAML, Pygments, click and rich are ports in `depends`; PyYAML and Pygments are
named in `pypackages` as well, so the bundle carries their sdists too, and pip installs neither.
See [The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

Build isolation stays on, pointed at the bundle: it also carries the build backends those
sdists name (setuptools, setuptools-scm, hatchling, poetry-core, flit_core, Cython and their
own requirements), and pip installs them into each sdist's temporary build environment, never
into the package. `pypackages` pins bitstring to one release.

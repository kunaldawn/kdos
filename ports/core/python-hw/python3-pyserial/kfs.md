## About
[pySerial]{.idx} gives Python access to serial ports: `serial.Serial` for a port, and the
`pyserial-miniterm` and `pyserial-ports` commands. It is pure Python with no dependencies. On
KDOS a USB serial adapter is reached through the `dialout` group and the tree's udev rules; see
[The device groups and the udev rules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).
[python3-esptool](#port-python3-esptool), [python3-obd](#port-python3-obd),
[python3-meshtastic](#port-python3-meshtastic), [printrun](#port-printrun) and the other ports
that talk to hardware over a serial line import it.

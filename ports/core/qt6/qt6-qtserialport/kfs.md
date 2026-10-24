## About
Qt Serial Port is the QtSerialPort library: `QSerialPort` opens a serial line, a USB serial
adapter among them, and sets its speed, framing and flow control; `QSerialPortInfo` lists the
ports present, through [eudev](#port-eudev)'s `libudev`. Radio, GPS and telescope-control programs
reach their hardware through it, and [qt6-qtpositioning](#port-qt6-qtpositioning) reads NMEA
receivers with it.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).

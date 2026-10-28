## About
Qt Positioning for Qt 6 is the QtPositioning library and its QML types: `QGeoPositionInfoSource`
reports where the machine is, through plugins. This port builds the GeoClue plugin, which asks
[geoclue](#port-geoclue) over D-Bus, the NMEA plugin, which reads a GPS receiver's NMEA
sentences from a serial port through [qt6-qtserialport](#port-qt6-qtserialport) or from a file,
and the `positionpoll` plugin, which watches for entering and leaving an area by polling a position
source.

## Flags
- `-DFEATURE_gypsy=OFF` — no plugin for Gypsy, a GPS multiplexing daemon that is not a port.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).

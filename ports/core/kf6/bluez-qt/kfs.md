## About

BluezQt is the KDE Frameworks wrapper around the D-Bus API of BlueZ 5: adapters, devices, pairing
agents, profiles and OBEX file transfer as Qt objects, with a QML module, `org.kde.bluezqt`, for
Qt Quick programs. Plasma's Bluetooth applet and settings page are built on it, and so is the
Bluetooth share plugin of [purpose](#port-purpose).

## Notes

The library speaks to `bluetoothd` and `obexd` over the system and session buses and links no
BlueZ library, so the configure looks for nothing of BlueZ's. [bluez](#port-bluez) is in `depends`
for run time: without the daemon every adapter list is empty.

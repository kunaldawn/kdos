## About

ModemManager finds and drives cellular modems: built-in WWAN cards, USB modems, and phones that
offer dial-up networking over Bluetooth. [networkmanager](#port-networkmanager) makes the
connection through it, and `mmcli` shows a modem's state, unlocks its SIM and reads its messages;
see [Mobile broadband](../../../../docs/kfs/16-wiki/mobile-broadband.md).

`ModemManager` is started at boot by `/etc/init.d/42_modemmanager.sh`, after polkit and before
NetworkManager, and never by D-Bus activation; see
[How polkit and ModemManager are started](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).

## Notes

The recipe deletes
`/usr/share/dbus-1/system-services/org.freedesktop.ModemManager1.service`, the D-Bus activation
file the build installs whatever its options. NetworkManager calls `org.freedesktop.ModemManager1`
as it starts, and with the file that call would make the system bus start a second ModemManager
outside the supervisor; whichever lost the name would exit, and a supervised one that lost would
be restarted every five seconds without ever getting it. Without the file the call is refused, and
`42_modemmanager.sh` waits until ModemManager holds the name before `42_networkmanager.sh` runs.

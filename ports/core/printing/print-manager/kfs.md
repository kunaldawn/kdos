## About

print-manager is Plasma's printing front end over [cups](#port-cups): the Printers page of System
Settings, which adds, configures and removes printers and sets the server's options; the print
queue window (`plasma-print-queue`); the printer options dialog (`configure-printer`); the tray
applet that shows active jobs; and a module of the KDE daemon that announces a newly plugged-in
printer and offers to set it up. Each part reaches CUPS through the port's own `libkcups`.

## Flags

- `-D SCP_INSTALL=OFF` — Do not offer to install system-config-printer. Upstream's option adds an
  "Install It" button that asks PackageKit for the package; KDOS installs it with this port
  instead (see Notes), and PackageKit is not a port.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON` — No PackageKit, which the installer above
  would use. Refusing the search means a stray copy in the build root cannot be picked up.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[system-config-printer](#port-system-config-printer) is in `depends` for run time. Its
`scp-dbus-service` answers `org.fedoraproject.Config.Printing` on the session bus, and
print-manager calls it to group the device URIs one physical printer reports
(`GroupPhysicalDevices`), to recommend a driver for a new printer (`GetBestDrivers`) and to list
the programs a driver lacks (`MissingExecutables`). The Printers page checks whether the name is
activatable and shows a warning header when it is not.

The configure looks for CUPS's `smb` backend under `/usr/lib/cups/backend` and reports it
missing: samba installs `smbspool` but the tree links no `smb` backend into CUPS, so a printer
shared over SMB cannot be added. The check is a run-time note and changes nothing in the build.

[kded](#port-kded) is in `depends` for run time, for the new-printer module that `kded6` loads at
login. The module claims `com.redhat.NewPrinterNotification` on the system bus and waits for
system-config-printer's udev helper to call it when a USB printer appears. system-config-printer
is built without its udev rules, so that call never comes and a plugged-in printer is added from
the Printers page instead.

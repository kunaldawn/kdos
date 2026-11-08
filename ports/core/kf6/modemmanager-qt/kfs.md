## About

ModemManagerQt is the Qt wrapper around ModemManager's D-Bus API: modems, their SIM cards, signal
quality, location, SMS messages and voice calls as Qt objects. Plasma's network applet shows mobile
broadband through it, and [kdeconnect](#port-kdeconnect) links it.

## Flags

- `-D BUILD_EXAMPLES=OFF` — Build none of the example programs. It is upstream's default, written
  out so the recipe states it.

## Notes

The configure requires ModemManager 1.0 or newer, found through pkg-config, for the headers that
give its D-Bus interface names and enumerations: [modemmanager](#port-modemmanager) and
[pkgconf](#port-pkgconf) are in `depends` for them. At run time the library talks to the daemon
over the system bus. The source carries no translations, so the recipe has no
`KF_SKIP_PO_PROCESSING`.

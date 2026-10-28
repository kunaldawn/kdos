## About

GeoClue is the D-Bus geolocation service: a program asks it on the system bus for the machine's
position, and it answers from Wi-Fi networks, the cellular modem, a GPS feed, the IP address or a
fixed location. KDOS carries it because it is what xdg-desktop-portal's Location portal asks, and
`libgeoclue`, the client convenience library, is what [xdg-desktop-portal](#port-xdg-desktop-portal)
links for it. How the service runs on KDOS, with no consent agent, is in
[Access, and what it unlocks](../../../../docs/kdos/03-architecture/session.md#access-and-what-it-unlocks).

## Flags

- `--libexecdir=/usr/lib` — Install the daemon as `/usr/lib/geoclue` and the demo programs under
  `/usr/lib/geoclue-2.0/demos`. The same path is written into the D-Bus service file's `Exec=`.
- `-Dlibgeoclue=true` — Build `libgeoclue`, which the Location portal links.
- `-Dintrospection=false`, `-Dvapi=false`, `-Dgtk-doc=false` — Generate no GObject introspection
  data, no Vala bindings and no API reference for `libgeoclue`; upstream turns all three on by
  default.
- `-Dwifi-source=true`, `-D3g-source=true`, `-Dcdma-source=true`, `-Dmodem-gps-source=true`,
  `-Dnmea-source=true`, `-Dstatic-source=true`, `-Dip-source=true`, `-Dcompass=true` — Build every
  location source. These are upstream's defaults, named so that the set the service is built
  with is in the recipe. The Wi-Fi and 3G sources ask beaconDB, upstream's default lookup service,
  over libsoup; the 3G, CDMA and modem GPS sources read [modemmanager](#port-modemmanager); the
  NMEA source finds a GPS feed on the local network through [avahi](#port-avahi); the compass is
  `iio-sensor-proxy` on the bus; the static source reads `/etc/geolocation`.
- `-Denable-backend=true` — Build the service itself, not only the client library.
- `-Ddemo-agent=false` — Build no demo agent. It is upstream's consent dialog, shown as a
  notification; on KDOS the question is the portal's own access dialog.
- `-Ddbus-srv-user=geoclue` — Run the service as the `geoclue` account rather than root. The name
  is written into the D-Bus service file's `User=`, into the bus policy that lets only that
  account own `org.freedesktop.GeoClue2` and ask `wpa_supplicant` for a scan (what the Wi-Fi source
  reads), and into the polkit rule that lets it switch on a modem's location through ModemManager.

## Patches

- `no-agent.patch` — Lets a client through when the configuration whitelists no agent at all. Upstream holds every `GetClient` call until a consent agent for the caller's user registers, with no timeout, and with no agent present makes `Start` wait out an agent start-up delay before it decides. With an empty `[agent]` whitelist no agent can ever register, so the portal's call would wait until D-Bus gave up on it. The patch skips the wait in `gclue-service-manager.c` and completes `Start` at once in `gclue-service-client.c` whenever the whitelist is empty.

## Notes

`build.sh` writes `/etc/geoclue/conf.d/90-kdos.conf`, which the service reads after its main
configuration. It empties the `[agent]` whitelist, the condition `no-agent.patch` tests, and gives
the portal, `[xdg-desktop-portal]`, `allowed=true` and `system=true`, so the Location portal is
treated as a system component for every user. The result is that GeoClue answers any client on
the system bus that is not refused by name; see
[known gaps](../../../../docs/kdos/06-reference/known-gaps.md#a-boxed-application-is-given-your-location-without-being-asked).

The `geoclue` account is made by `postinstall.sh`, under `PKG_ROOT`, the root the package is being
installed into. `dbus-daemon-launch-helper` refuses to start a service whose `User=` does not
exist, so without the account the first client's request fails. Its home is `/nonexistent`:
geoclue writes no files. The hook creates the account only when `etc/passwd` lacks it, and then
sets the home with `usermod` whether it created the account or not, so a tree that already had a
`geoclue` account with another home is given `/nonexistent` as well. The `sysusers.d` file upstream
installs for the same account is removed with the `systemd` directory: both are systemd's.

`where-am-i`, the demo client, stays installed as a command under `/usr/lib/geoclue-2.0/demos`.
Its desktop entry, which upstream marks `NoDisplay=true`, is removed.

D-Bus starts the service when the first client asks, and it exits after a minute with none, so
there is no init script.

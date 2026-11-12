## About

Discover is Plasma's software centre. On KDOS it is the graphical face of two update paths and
nothing else: operating-system updates over the A/B slots, through
`SystemdSysupdateBackend` talking to `org.freedesktop.sysupdate1` on the system bus, and firmware
updates through [fwupd](#port-fwupd). It also carries the Software Update page of System Settings
and the tray notifier that announces pending updates. There is no third-party store: no Flatpak, no
Snap, no PackageKit and no KNewStuff add-ons (see
[Offline by construction](../../../../docs/kdos/01-philosophy/principles.md#offline-by-construction)).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_SystemdSysupdateBackend=ON` — Build the backend for `org.freedesktop.sysupdate1`.
  Upstream's default is off. KDOS serves that interface over its A/B slots, so this backend is how
  Discover lists, downloads and installs a new system image; without it Discover has no operating
  system updates to show.
- `-D BUILD_FwupdBackend=ON` — Build the firmware backend. It is upstream's default, but the
  backend is still dropped in silence when the `fwupd` pkg-config module is missing, so the last
  line of `build.sh` checks that it was built.
- `-D BUILD_FlatpakBackend=OFF` — No Flatpak backend. Upstream's default is on; Flatpak is not a
  port and KDOS installs nothing from a store at run time.
- `-D BUILD_SnapBackend=OFF` — No Snap backend, for the same reason. Upstream's default is on.
- `-D BUILD_AlpineApkBackend=OFF` — No backend for Alpine's package manager. Upstream's default is
  on and only the absence of ApkQt keeps it out; naming it off means a stray ApkQt in the build root
  cannot bring it in.
- `-D BUILD_PackageKitBackend=OFF` — No PackageKit backend. PackageKit is not a port, and KDOS
  packages are not installed one at a time on a running system.
- `-D BUILD_RpmOstreeBackend=OFF` — No rpm-ostree backend; KDOS has no OSTree deployments.
- `-D BUILD_HoloBackend=OFF` — No backend for SteamOS's image updater.
- `-D BUILD_DummyBackend=OFF` — No test backend, which would show invented applications.
- `-D BUILD_AppStreamPreviewBackend=OFF` — No backend that opens a loose AppStream file as an
  installable entry. Upstream defaults it to whether AppStreamQt was found, which on KDOS is always.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6NewStuff=ON` — Turns off the KNewStuff backend, which has no
  option of its own: it is built whenever KF6Attica, KF6NewStuff and AppStreamQt are all found, and
  all three are ports. That backend downloads themes, widgets and scripts from store.kde.org.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — No KUserFeedback, which Discover uses to
  submit telemetry. It is not a port; refusing it means a stray copy in the build root cannot be
  picked up.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Qt6WebView=ON` — No Qt WebView. Discover uses it only for the
  web sign-in a Flatpak remote can ask for, and the Flatpak backend is not built; found, it would
  link Qt WebEngine into Discover for a dialog that never opens.
- `-D WITH_KCM=ON` — Build the Software Update page of System Settings (`kcm_updates`), where
  automatic updates and the notification frequency are set. It is upstream's default, named so the
  page cannot disappear unnoticed.
- `-D WITH_NOTIFIER=ON` — Build `DiscoverNotifier`, the tray notifier started from
  `/etc/xdg/autostart`. It is upstream's default, named for the same reason.

## Patches

`odrs-offline.patch` adds `[Software] FetchOdrsRatings` to `discoverrc`, read by the ratings
backend before it downloads anything; upstream fetches every application's ratings from
odrs.gnome.org at each start whose cached copy is a day old, for the Home page's "Most Popular"
list. `/usr/share/kdos/xdg/discoverrc` sets it false. The key defaults to true, so the patch changes
nothing where it is unset, and it is proposed upstream in that form.

`sysupdate-errors.patch` makes the system-update backend read the error replies of
`org.freedesktop.sysupdate1`. Upstream reads `GetVersion` and `CheckNew` straight into a string,
and an error reply converts to its own message, so a refused check (kdos-sysupdated's
`Interactive authentication required`) is listed as an update to a version of that name. With
the patch an error reply is a passive message and the target gets no resource, and a failed
`ListTargets` ends the fetch it began. The patch also sets "restart to finish" only when the
Install job ends with status 0, so a refused or failed Acquire leaves nothing pending; and it asks
the size service upstream expects at `http://localhost:3129` once per update, leaving the size
empty when nothing answers there, which on KDOS is always, so an update with no size of its own
shows no size row instead of a busy indicator that never stops. It is to be offered upstream.

`sysupdate-size.patch` makes the system-update backend take an update's size from the AppStream
file the target names: the download size of an artifact of the release whose version is the one
offered. sysupdate1 has no size query, so upstream asks only the size service above;
[kdos-sysupdated](#port-kdos-sysupdated) writes that artifact from the update document, and the
Updates list shows the size and its total instead of "Unknown". It applies after
`sysupdate-errors.patch` and is to be offered upstream.

`update-failed-stays-listed.patch` reports a transaction that ended with an error to the Updates
page as not started, as a cancelled one is, instead of done. The page hides rows whose state is
done, and keeps its rows when the list of offered updates is unchanged, which it is after a
refused or failed update; without the patch the page lists nothing, with Update All still
enabled, until Refresh. It is to be offered upstream.

`ratings-need-reviews.patch` shows the application page's rating row and its Reviews section,
with "Write a Review", only when the resource's backend has a reviews backend, and leaves the Home
page's "Highest-Rated Games" and "Highest-Rated Developer Tools" rows empty when the current
application backend has none. Neither KDOS backend has one, so on KDOS no page offers a rating or
a review it has nowhere to send, and no row is ranked by ratings nobody gave. It is to be offered
upstream.

## Notes

The last line of `build.sh` fails the build unless `/usr/lib/qt6/plugins/discover/` holds exactly
`fwupd-backend.so` and `systemdsysupdate-backend.so`. A backend that appears or goes missing there
is a change to what Discover offers, so it stops the build instead of shipping.

`build.sh` removes two hidden desktop entries upstream installs whatever backends are built:
`org.kde.discover.snap.desktop`, which claims `snap:` links, and
`org.kde.discover.appstreampreview.desktop`, which claims loose AppStream files. With neither
backend built, each would open a Discover window with nothing in it, and claim a type for a program
that cannot handle it (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).

The firmware backend refreshes the metadata of every enabled remote that is neither local nor a
directory each time Discover starts (`FwupdBackend::setRemotes`). [fwupd](#port-fwupd) installs its
LVFS remote switched off and lists firmware from the `kdos-snapshot` local remote, so starting
Discover downloads nothing; once `fwupdmgr enable-remote lvfs` turns the LVFS on, every start of
Discover downloads its metadata.

Three of Discover's own paths reach the network whatever backends are loaded. The ratings fetch is
turned off by `odrs-offline.patch`. The featured list is read from the URL in
`/usr/share/discover/featuredurlrc`, which [kdos-discover](#port-kdos-discover) installs pointing at
a local file, instead of `autoconfig.kde.org`. The firmware backend's refresh is the paragraph
above.

libdiscover is private to Discover and installs no header, so `build.sh` copies its headers, and
the three the build generates (`DiscoverConfig.h` with the plugin IIDs, and the two export headers),
to `/usr/include/discover`. [kdos-discover](#port-kdos-discover) builds its backend and its notifier
plugin against them. The IIDs embed the full version (`org.kde.discover.6.8.2.…`), and Discover's
loader skips a plugin whose IID differs, so every discover version bump, point releases included,
rebuilds kdos-discover and changes its `KDOS_DISCOVER_VERSION`. Upstream keeps libdiscover private
on purpose, so the header installation stays KDOS's.

The system-update backend only pings `org.freedesktop.sysupdate1` to decide whether it is usable,
so it finds KDOS's own `kdos-sysupdated`, which owns that name on the system bus, with no patch. Discover's
packaging-library plugins are versioned by the exact Plasma release (`DISCOVER_PLUGIN_IID`), so a
backend built against another Discover release is not loaded.

The configure also finds the `libmarkdown` pkg-config module when [discount](#port-discount) is
installed; only the PackageKit backend uses it, so its presence changes nothing that is built.

[kirigami](#port-kirigami), [kirigami-addons](#port-kirigami-addons),
[kitemmodels](#port-kitemmodels) and [purpose](#port-purpose) are QML modules Discover loads at
run time; the configure checks only for `org.kde.kitemmodels`.

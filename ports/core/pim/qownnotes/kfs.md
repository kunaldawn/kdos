## About

[QOwnNotes]{.idx} is a Qt 6 note-taking program that keeps notes as plain Markdown files in a
folder, with a live HTML preview, to-do lists, and optional synchronisation with a Nextcloud or
ownCloud server. Passwords for those servers are kept in the Secret Service through
[libsecret](#port-libsecret).

## Flags

- `-D QON_QT6_BUILD=ON` — Build against Qt 6. Upstream's default is Qt 5.
- `-D BUILD_WITH_SYSTEM_BOTAN=OFF` — Compile the Botan crypto library from the copy under
  `libraries/botan`, upstream's default, rather than link the system Botan 3 of
  [botan](#port-botan).
- `-D BUILD_WITH_LIBGIT2=OFF`, `-D BUILD_WITH_ASPELL=OFF` — Link neither libgit2 nor Aspell;
  both are off by default upstream.
- `-D USE_QLITEHTML=ON` — Render the preview with QLiteHtml, the litehtml engine compiled from
  `libraries/`, upstream's default with Qt 6.

## Patches

- `no-telemetry-no-update-check.patch` — returns at the start of every `MetricsService` call that sends a visit, an event or the heartbeat to the Piwik usage tracker, and makes `UpdateService::checkForUpdates` return unless the check is a manual one. Help, Check for updates still asks when the user chooses it.

## Notes

QtKeychain, FakeVim, litehtml, md4c and the rest under `libraries/` are compiled from those copies;
apart from Botan, the build has no switch to take any of them from the system.

libsecret is looked up with no switch, and without it QtKeychain cannot reach the Secret Service,
so passwords could not be stored. `build.sh` checks `LIBSECRET_FOUND` in `CMakeCache.txt` after
the configuration and stops the build if it is missing.

The translations under `/usr/share/QOwnNotes/translations` are removed: English is the source text
of the interface.

`build.sh` replaces upstream's desktop entry. `setDesktopFileName` makes the Wayland app id
`PBE.QOwnNotes`, not upstream's `QOwnNotes`, and the new entry names it in `StartupWMClass`.

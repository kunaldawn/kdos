## About

[KPatience]{.idx} (`kpat`) is KDE's collection of solitaire card games: Klondike, Spider,
FreeCell, Yukon, Golf, Simple Simon and the rest of upstream's list, with hints, demo play and a
solver that says whether a deal can be won. It is a Qt 6 and KDE Frameworks application, and it
draws its cards from the decks [libkdegames](#port-libkdegames) installs under
`/usr/share/carddecks`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D BUILD_DOC=ON` — build the KPatience handbook and the `kpat(6)` manual page with
  [kdoctools](#port-kdoctools), so Help opens the handbook. Upstream's default, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — make the KF6DocTools lookup required at the
  `find_package` call itself, so a build root without kdoctools stops the configure there.
- `-D WITH_BH_SOLVER=ON` — use [black-hole-solver](#port-black-hole-solver) to solve Golf, which
  upstream then requires. The solver for FreeCell, Simple Simon, Baker's Dozen and Castle,
  [freecell-solver](#port-freecell-solver), is required whatever this switch says. Upstream's
  default, named, so Golf cannot be left without its solver.

## Notes

`kdoctools_install(po)` builds and installs every translated handbook and every translated
`kpat(6)` page that the release carries, and this happens even with `KF_SKIP_PO_PROCESSING` on.
`build.sh` deletes every directory in `/usr/share/man` that is not a `man<section>` one, which
removes the translated manual pages, and kpkg's prune removes the same directories again together
with the translated handbooks under `usr/share/doc/HTML/<lang>/`, so Help and `man kpat` open the
English copies.

`build.sh` writes its own `org.kde.kpat.desktop` over upstream's to add
`StartupWMClass=org.kde.kpat`: KDE's `KAboutData` sets the desktop file name, so the window's
Wayland app id is `org.kde.kpat`. The entry keeps upstream's `Exec`, MIME type for saved games and
handbook path, and adds a `TryExec=kpat` upstream's lacks. It drops the per-game actions upstream
lists for starting one game directly, and the translations; its keywords are a list of its own. The
hicolor icons it names are upstream's.

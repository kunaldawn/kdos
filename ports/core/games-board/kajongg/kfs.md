## About

Kajongg is the classical game of Mah Jongg for four players, not the solitaire: it plays the whole
game with scoring, several rulesets, computer players and voices, and it can host or join a table
over the network. It is written in Python and draws through PyQt6, by way of the qtpy wrapper;
`kajonggserver` is the game server every table runs on.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  `kdoctools_install()` still builds the handbook in every translated language, and kpkg's
  language rule removes `/usr/share/doc/HTML/<lang>/` from the package.

## Notes

Nothing is compiled. The install copies the Python sources to `/usr/share/kajongg` and links
`kajongg` and `kajonggserver` in `/usr/bin` to the two scripts there. Upstream byte-compiles each
module at build time into `/usr/share/kajongg/__pycache__`; kpkg's own checked-hash compile then
rewrites every one of those `.pyc` files (see [What kpkg does around
build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).

The configure runs `python3` to import Twisted and stops without it, so
[python3-twisted](#port-python3-twisted) is a build dependency as well as a run-time one. It also
requires `KMahjongglib6` from [libkmahjongg](#port-libkmahjongg); Kajongg links nothing from it and
reads its tile sets and backgrounds at run time.

The remaining dependencies are run-time ones that the build does not check. Kajongg asks
[python3-qtpy](#port-python3-qtpy) for PyQt6, from [python3-pyqt6](#port-python3-pyqt6), unless
`QT_API` in the environment names another binding. The tiles and backgrounds are drawn through
PyQt6's `QtSvg` and `QtSvgWidgets` modules, over [qt6-qtsvg](#port-qt6-qtsvg), which the configure
requires as well. The voices are Ogg files played by `ogg123` from
[vorbis-tools](#port-vorbis-tools); without it the voices are switched off with a warning. `sox`
from [sox](#port-sox) moves each player's voice to the left or the right speaker; without it every
voice comes from both.

A game on one machine connects Kajongg to its `kajonggserver` over a Unix socket in
`~/.kajonggserver`. A table other machines join listens on TCP, 8303 by default, which the
firewall's input chain drops until a rule opens it (see [The
firewall](../../../../docs/kdos/03-architecture/security-model.md#the-firewall)).

The source's `bash_completion.d/kajongg` is not installed: the build does not install it, and it
finds the game database through KDE 4's `kde-config`, which no port provides.

The window's Wayland app_id is `org.kde.kajongg`: `kajongg.py` sets that desktop file name itself.
It is the desktop entry's file name too, and upstream's entry sets no `StartupWMClass`, so the task
manager pairs the window with its entry without a patch.

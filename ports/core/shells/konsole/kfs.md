## About

Konsole is KDE's terminal emulator and the default terminal of the KDOS session: tabs and split views,
profiles and colour schemes, bookmarks, search, an SSH manager and quick commands in side panels,
and sixel, iTerm2 and kitty inline pictures. Its KPart is the terminal panel inside
[dolphin](#port-dolphin) and [kate](#port-kate).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D USE_DBUS=ON` — Builds the D-Bus interface through which `konsole` opens a new window or tab
  in the instance already running, and scripts drive sessions. Upstream's default on Linux, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Requires [kdoctools](#port-kdoctools) for the
  handbook, which upstream builds only when it finds kdoctools.
- `-D WITH_X11=ON` — Builds the code paths Konsole takes when it runs as an X11 client under
  Xwayland: its compositing check, window activation and placement through KX11Extras, and the
  `--display`, `--visual` and `--waitforwm` options. Upstream's default, named; as a Wayland
  client it skips them at run time.
- `-D WITH_LIBSSH=ON` — Reads `~/.ssh/config` through [libssh](#port-libssh), which the SSH
  manager imports hosts from. Upstream's default, named; it makes libssh a requirement.
- `-D WITH_KAPSULE=OFF` — No Kapsule container integration, which needs libkapsule-qt; it is not a
  port. Upstream's default, named.
- `-D ENABLE_PLUGIN_SSHMANAGER=ON`, `-D ENABLE_PLUGIN_QUICKCOMMANDS=ON` — Build the SSH manager
  and quick commands side panels. Upstream's defaults, named.
- `-D INSTALL_ICONS=ON` — Installs Konsole's own application icons into hicolor, so the
  `utilities-terminal` icon its desktop entry names resolves whatever icon theme is chosen.
  Upstream's default is off.

## Notes

`kdoctools_install()` builds the handbook in every translated language the tarball carries, and
kpkg's language rule removes the translated ones from the package. The loop at the end of
`build.sh` removes every directory under `/usr/share/man` that is not a `man<section>` one, where
`kdoctools_install()` puts a translated manual page; Konsole ships no manual page, so the loop
finds nothing to remove.

Konsole is the default terminal of the session: the `TerminalApplication` and `TerminalService`
keys in the shipped `kdeglobals`, and the KDOS profiles in the shipped `konsolerc`, name it. Both
files are KDOS's configuration, not this package's.

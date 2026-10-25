## About

Goxel is a 3D voxel editor: a model is built out of cubes on a grid, painted, and exported to
formats such as glTF, OBJ, PLY, MagicaVoxel `.vox` and PNG slices. It draws its whole interface
itself with OpenGL through [glfw](#port-glfw), so it links no toolkit, and it builds with
[scons](#port-scons).

## Flags

- `mode=release` — Upstream's default mode is `debug`, which compiles with `-O0` and links
  AddressSanitizer and UndefinedBehaviorSanitizer into the program. `release` drops both and
  compiles with `NDEBUG` and `-Ofast`. The SConstruct itself appends the exported `CFLAGS`, `CXXFLAGS`
  and `LDFLAGS` (see [the release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)). SCons puts its `CCFLAGS`, where `-Ofast` is, after `CFLAGS` and `CXXFLAGS` on each
  compile line, so `-Ofast` overrides the exported `-O2`.
- `werror=false` — Upstream's default is `true`, which adds `-Werror`, so any warning the tree's
  compiler raises that upstream's did not would stop the build.
- `nfd_backend=portal` — The open and save dialogs go through the desktop's FileChooser
  [portal](../../../../docs/kdos/03-architecture/session.md#portals) over D-Bus (upstream's
  `nfd_portal.cpp`, linked against [dbus](#port-dbus)) instead of the default `gtk` backend, which
  links GTK 3 to draw its own dialog. On KDOS the portal opens the desktop's own file chooser, and
  `gtk3` is not in `depends`.
- `sound=false` — No sound effects, so no link to OpenAL. This is upstream's default; naming it
  keeps OpenAL out of the link whatever that default becomes.

## Patches

- `goxel-app-id.patch` — In `src/main.c`, before the window is created, sets the GLFW hints `GLFW_WAYLAND_APP_ID` and `GLFW_X11_CLASS_NAME` to `goxel`. Without the first, GLFW gives the Wayland window an empty app id, and the panel cannot match the window to `goxel.desktop` (see [`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)). GLFW uses the X11 class only when an instance name is set as well, so on X11 the second hint changes nothing.

## Notes

`make install` is upstream's Makefile, which takes `PREFIX` and `DESTDIR`; it installs the binary,
the hicolor icons, the AppStream file and upstream's desktop entry. `build.sh` then writes its own
`goxel.desktop` over that entry, with `%f` in `Exec`, `TryExec`, `StartupWMClass=goxel` and search
keywords.

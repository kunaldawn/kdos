## About

libinput turns the kernel's evdev input events into what a Wayland compositor needs: pointer
motion with acceleration, touchpad tapping, scrolling and gestures, palm and thumb detection,
tablets and switches such as a laptop lid. KDOS's compositor, `kdos-comp`, gets all of its input
through it, by way of [wlroots](#port-wlroots); the compositor learns of a closing lid from
libinput, and no other program on KDOS watches the lid switch. [kdos-comp](#port-kdos-comp),
[qt6-qtbase](#port-qt6-qtbase) and [qt5-qtbase](#port-qt5-qtbase) depend on it as well. The
touchpad settings a user can change are the `<libinput>` block of the compositor's configuration;
see [Changing the bindings](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/desktop.md#changing-the-bindings).

libinput sizes many touchpads from the `EVDEV_ABS_*` entries of udev's hardware database, so it
depends on the image phase compiling that database; see
[Databases stamped into the image](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-system.md#databases-stamped-into-the-image).

## Flags

- `-Db_ndebug=false` — Keeps `assert()` compiled in. libinput's own header warns with `#warning
  "libinput relies on assert(). #defining NDEBUG is not recommended"`, and its quirks parser
  undefines `NDEBUG` itself.
- `-Dudev-dir=/lib/udev` — Where the udev helpers (`libinput-device-group`,
  `libinput-fuzz-extract`, `libinput-fuzz-to-zero`) and the rules that call them go. It is the
  directory the [eudev](#port-eudev) port installs its own rules under; `/lib` is a link to
  `/usr/lib`, so it is the same directory as upstream's default, `$prefix/lib/udev`.
- `-Ddebug-gui=false` — Builds no `libinput debug-gui`, the GTK window that draws input events.
  It needs GTK, and `41_system`, the phase that builds libinput, holds only ports with no toolkit
  in their dependency closure.
- `-Dtests=false`, `-Ddocumentation=false` — Build no test suite and no HTML documentation.
- `-Dmtdev=true` — Uses [mtdev](#port-mtdev) to translate the old multitouch protocol A into
  protocol B for the devices that still send it. It is upstream's default.
- `-Dlua-plugins=enabled`, `-Dautoload-plugins=true` — Build support for plugins written in Lua,
  from [lua54](#port-lua54), and load the plugins found in libinput's plugin directories
  (`/usr/lib/libinput/plugins` and `/etc/libinput/plugins`) whenever the program using libinput
  does not load them itself. Upstream leaves autoloading off. `enabled` makes a missing Lua a
  configure error, where `auto` would build without plugin support.
- `-Dlibwacom=true` — Identifies graphics tablets through [libwacom](#port-libwacom)'s database
  of tablet models. It is upstream's default.

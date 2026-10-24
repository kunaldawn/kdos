## About

Wine runs Windows programs on Linux by providing the Windows API itself, not by emulating a
machine. The port is a WoW64 build: one 64-bit loader runs both 64-bit and 32-bit Windows
programs, and no 32-bit Linux library is needed. [wine-mono](#port-wine-mono) and
[wine-gecko](#port-wine-gecko), Wine's .NET runtime and HTML engine, are in `depends`; they install
under `/usr/share/wine`, where Wine looks for them when it sets up a prefix.

## Flags

- `CROSSCFLAGS="-O2 -pipe"` — The compile flags for the Windows (PE) half of Wine. They replace
  upstream's default, `-g -O2`, so the DLLs carry no debug information.
- `--enable-archs=x86_64,i386` — Build the PE side for both architectures in one tree, which is
  the WoW64 layout: the Unix side is 64-bit only, and the i386 DLLs run under it.
- `--with-mingw=clang` — Compile the PE side with [clang](#port-clang) in its MSVC-compatible mode,
  linked by [lld](#port-lld). Wine carries its own C runtime, so no MinGW toolchain or headers are
  involved; without the value configure searches for a `*-w64-mingw32-gcc` first.
- `--disable-tests` — Do not build the regression tests.
- `--with-wayland` — This switch and every `--with-` after it are libraries Wine uses whenever it
  finds them; naming one turns a missing library into a configure error (`This is an error since
  --with-… was requested`) instead of a notice. Between them they are both display drivers
  (`--with-wayland` and `--with-x` with its X extensions), OpenGL, Vulkan and OpenCL, ALSA and
  PulseAudio sound, CUPS printing, D-Bus and udev device hot-plugging, libusb, SDL game
  controllers, V4L2 cameras, gPhoto digital cameras, SANE scanners, libpcap, PC/SC smart cards,
  GnuTLS for Schannel, Kerberos and GSSAPI, GStreamer and FFmpeg codecs, fontconfig and FreeType,
  inotify, libunwind and pthreads.
- `--without-hwloc` — Do not use hwloc for the CPU and cache topology; upstream's help text says
  that path is FreeBSD's. On Linux Wine reads the topology from sysfs.
- `--without-oss` — No OSS sound driver. OSS is not on this system.
- `--without-capi` — No CAPI, the ISDN interface.
- `--without-coreaudio` — No CoreAudio driver, which is macOS's.
- `--without-netapi` — Do not use Samba's NetAPI library.

## Patches

- `rpath-origin.patch` — Adds a check to `configure.ac`, and the same check generated into `configure`, that the compiler accepts `-Wl,-rpath,$ORIGIN`, and when it does adds that run path to `UNIXLDFLAGS`, the link flags of Wine's Unix libraries. Those libraries, under `/usr/lib/wine/x86_64-unix`, name each other in `DT_NEEDED` by bare file name. musl's loader matches an already-loaded library only when it was found by a search, not by the path Wine `dlopen`s it with, so without an `$ORIGIN` run path every one of them fails to load.

## Notes

Both display drivers are built. Wine tries x11 before wayland and the session always sets
`DISPLAY`, so `winex11` under Xwayland is the default, and a Vulkan surface there needs the
[vulkan-loader](#port-vulkan-loader)'s xcb and xlib platforms. `winewayland` is chosen per prefix
with `Graphics=wayland` under `HKCU\Software\Wine\Drivers`, or for one run by starting `wine` with
`DISPLAY` unset.

`build.sh` links `wine64` to `wine`, for scripts that call `wine64`; in WoW64 the one loader is
both. Only the English manual pages are kept: the `*.UTF-8` directories under
`/usr/share/man` are removed.

The program loader's entry has `NoDisplay=true` and only opens a Windows program from the file
manager. `winecfg` is the one menu row: a prefix is made on first use, and every other Windows
program is reached through the file it opens or `wine <program>`. Its window class is its
executable name, `winecfg.exe`.

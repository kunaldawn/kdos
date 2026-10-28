## About
[BRLTTY]{.idx} is the screen reader of the text console: it drives a refreshable braille display,
speaks through a speech synthesiser, or both. It reads the Linux console through `/dev/vcsa`, so
what it covers is the login prompt, a shell on another virtual terminal and the installer. It does
not read the graphical session. There the reader is Orca, which reads the native applications and
reaches a braille display through BRLTTY's API, BrlAPI, and its Python module. What is and is not
read, how to turn it on and how its speech reaches the sound card are in
[Braille and speech at a text console](../../../../docs/kdos/02-user-guide/accessibility.md#braille-and-speech-at-a-text-console).

The package installs the `brltty` daemon, with its three screen drivers and the eSpeak NG speech
driver built in; its braille drivers and its other speech drivers as loadable modules under
`/usr/lib/brltty`; `libbrlapi` with its Python module; the polkit action and rule that admit a
BrlAPI client; and a service script, `65_brltty`.

## Flags
- `--disable-x` — leave out BRLTTY's X11 support: `xbrlapi`, the helper that tells BrlAPI which X
  window has the focus, and the `XWindow` braille driver, which draws a braille display in an X
  window. This desktop is not an X desktop, and no Xorg server is part of KDOS
  (see [No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
- `--enable-python-bindings` — build the `brlapi` Python module, which is how Orca drives a
  braille display. It is Cython, compiled with [python3-cython](#port-python3-cython). configure
  answers a missing Cython or Python environment with a warning and builds without the module, so
  `build.sh` checks for `brlapi*.so` after the install and fails the build when it is absent.
- `--disable-java-bindings` — and the OCaml, Tcl, Lua, Emacs and Lisp bindings after it: the other
  languages BrlAPI can be called from, none of which anything in the tree uses.
- `--enable-liblouis` — let a contraction table be a [liblouis](#port-liblouis) table, named with
  the `louis:` prefix, as in `-c louis:en-ueb-g2.ctb`. Grade 2 and every other contracted braille
  comes from liblouis; without it only BRLTTY's own few contraction tables exist (see
  [Contracted braille](../../../../docs/kdos/02-user-guide/accessibility.md#contracted-braille)).
- `--disable-gpm` — no mouse support through GPM, which is not a port.
- `--enable-i18n` — with `--enable-icu`, `--enable-polkit` and `--enable-expat`: translated
  messages, Unicode character handling through [icu](#port-icu), BrlAPI authorisation through
  [polkit](#port-polkit), and the XML parser for the CLDR tables through [expat](#port-expat). Each
  of these allows a feature and does not require it: configure builds without one whose library is
  missing. What makes each certain is its port in `depends`.
- `--with-curses=ncurses` — the curses library the TTY braille driver and `brltty-pty` use.
  BRLTTY's `ncursesw` choice includes `<ncursesw/ncurses.h>`, and this tree's ncurses installs its
  wide-character headers straight into `/usr/include`.
- `--with-rgx-package=libpcre2-32` — the regular expression library, PCRE2's library for 32-bit
  code units, named rather than taken first-found from configure's list, `libpcre2-32 libpcre32`.
- `--with-pcm-package=alsa` — with `--with-midi-package=alsa`: play BRLTTY's alert tunes through
  ALSA, as PCM and as MIDI, rather than through OSS, the list's other entry.
- `--with-service-package=no` — no system-service integration. Its one candidate on Linux is
  libsystemd, which KDOS does not have (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)); without it the
  daemon is an ordinary one that the service script supervises.
- `--without-libbraille` — keep out the driver for Libbraille, a third-party braille library that
  is not a port. configure registers that driver only when the library is found.
- `--without-espeak` — with `--without-flite`, `--without-mikropuhe`, `--without-swift` and
  `--without-theta`: keep out the speech drivers for engines this tree does not carry. The old
  eSpeak driver needs this switch most, because it links `-lespeak` and would otherwise be
  registered on the strength of the compatibility header [espeak-ng](#port-espeak-ng) installs in
  `/usr/include/espeak`.
- `ac_cv_header_eci_h=no` — keep out the ViaVoice speech driver, which has no switch and is
  registered whenever an `eci.h` header is found.
- `ac_cv_lib_intl_main=no` — answer configure's probe for a separate `libintl` with no, so the
  build never links the [libintl](#port-libintl) port when it happens to be installed. musl's own
  gettext functions carry the translations.
- `--with-screen-driver=lx,em,tx,-all` — build the three screen drivers into the daemon: `lx`
  reads the Linux console, `em` (TerminalEmulator) a terminal run under `brltty-pty`, and `tx` a
  `tmux` session. A driver the list does not name is built as a loadable module unless the list
  ends in `-all`, which leaves every other one out; among them is FileViewer, which links
  `-ltinfo`, a library this tree's ncurses does not build. The AtSpi2 driver, which would read a
  graphical desktop, is left out with them.
- `--with-speech-driver=eSpeak-NG` — build the eSpeak NG driver into the daemon. The name is
  matched case-sensitively against the directory under `Drivers/Speech`, and anything else is
  `unknown speech driver`. The list has no `-all`, so every other speech driver configure
  registered is built as a module; among them is SpeechDispatcher, which speaks through
  [speech-dispatcher](#port-speech-dispatcher).
- `INSTALL_ROOT=$PKG` — the staging directory. BRLTTY's makefiles read their own variable for it
  and not `DESTDIR`; given `DESTDIR` instead, the install would write into the live root, outside
  the package.
- `CONFLIBDIR=:` — the command run on the library directory after the API library is installed.
  configure writes `/sbin/ldconfig -n` for every Linux host without looking; `:`, the value it
  gives every other host, runs nothing, and musl's loader keeps no cache for `ldconfig` to
  rebuild.

The braille driver list is left at its default, which builds every driver configure registered as
a loadable module under `/usr/lib/brltty`; almost all of them speak to the display through BRLTTY's
own serial, USB, HID and Bluetooth code and need no library.

## Notes
The source is the release archive from brltty.app, not the tag archive from GitHub. The release
carries upstream's generated `configure`; the tag archive has none and needs `./autogen`, which
would generate it with this tree's autoconf and run `Tools/gendeps`, a Tcl script that opens every
source file to follow its includes. Upstream's `configure` calls `expr length`, a GNU extension
that POSIX `expr` does not have, so [coreutils](#port-coreutils) is in `depends`.

BrlAPI admits a client only when it may: its default is a key file plus polkit, and `make install`
writes neither half. The key file would be one secret copied into every image, so the port installs
only the polkit half, with `make install-polkit`: the action `org.a11y.brlapi.write-display` and
upstream's rule, which grants it to members of the `brlapi` group and tests no session.
`postinstall.sh` creates that group the first time the package is installed and adds the desktop
account, `kdos`, to it; a user later removed from the group stays out across reinstalls. Without
the group every BrlAPI client is refused, Orca and `brltty-clip` included (see
[Programs that talk to BRLTTY](../../../../docs/kdos/02-user-guide/accessibility.md#programs-that-talk-to-brltty)).

The service script, `/etc/init.d/65_brltty.sh`, starts nothing until `/etc/brltty.conf` exists
and is not empty, and nothing installs that file: an attached serial or USB device is no sign of a
braille display. It runs the daemon as root, because `/dev/vcsa` belongs to root and the `tty`
group, and sets `KDOS_ALSA_DEFAULT=kdos_card`, so speech goes straight to the sound card when no
session's PipeWire is running (see
[Turning BRLTTY on](../../../../docs/kdos/02-user-guide/accessibility.md#turning-brltty-on) and
[Speech and the sound card](../../../../docs/kdos/02-user-guide/accessibility.md#speech-and-the-sound-card)).

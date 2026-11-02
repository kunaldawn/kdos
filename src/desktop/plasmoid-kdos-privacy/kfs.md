## About
`plasmoid-kdos-privacy` is the Privacy Lamps applet, `org.kdos.privacy`, a compiled Plasma applet
in the system tray. It shows three lamps, microphone, camera and screen, each only while
something is capturing it, and names what is: "Firefox in box www". A click on a lamp mutes every
microphone capture (or unmutes them, when all already are), or stops every camera or screen
capture that can be stopped; the middle button, or Show What Is Capturing in its menu, opens the
list, with a Mute or Stop button for each capture. While nothing captures, the applet is passive
and the tray keeps it out of sight.

What is capturing comes from `kdos privacy --watch --json`, which kdos-tools provides: one line of
JSON at once and another each time a capture starts, stops or is muted. The helper reads
PipeWire's streams for the microphone and the screen, the holders of `/dev/video*` and PipeWire's
camera nodes for the camera, the desktop portal's screencast sessions, and each process's box from
its cgroup scope. The applet does not poll: it keeps the helper running and starts it again when
it exits, waiting longer each time it fails quickly. Muting and stopping run
`kdos privacy mute|unmute|stop <id>` for a capture the helper listed, and only what the helper says
can be done to it.

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no
  tarball, and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the applet into Qt's own plugin directory,
  `/usr/lib/qt6/plugins/plasma/applets`, where plasmashell looks for compiled applets.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/`, which
  drives a stand-in `kdos` from `tests/fake`, is run by the Qt regression net in the development
  container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build.

## Notes
The applet is `EnabledByDefault` with a notification-area category, which is what puts it in
every tray without a layout naming it. Plasma's own microphone indicator, plasma-pa's status
item `microphone`, and its camera indicator applet, `org.kde.plasma.cameraindicator`, name the
program but never its box, so the KDOS tray configuration hides both and only these lamps light.

An id from the helper goes back to it as one argument of its own. One that starts with `-` would
be read as an option, so such an entry is dropped rather than shown with a button that cannot
work. A line of a version the applet does not know is ignored: a helper whose fields change
meaning leaves the lamps dark rather than wrong. When the helper exits 2, the `kdos` installed has
no `privacy` command; the tooltip says so and the applet does not start it again.

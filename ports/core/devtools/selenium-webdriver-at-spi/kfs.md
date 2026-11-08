## About

KDE's WebDriver for AT-SPI2. A Flask service, `selenium-webdriver-at-spi.py`, answers the W3C
WebDriver protocol by reading and operating Qt applications through their accessibility tree, and
three small Qt programs do what the tree cannot: `selenium-webdriver-at-spi-inputsynth` types and
clicks through KWin's fake-input protocol, `-screenshotter` takes a screenshot through KWin's
ScreenShot2, and `-recorder` records through KPipeWire. KDOS runs it in the rig's guest:
`testing/atspi/guest.sh --webdriver` serves it inside the Plasma session and dumps each KDOS
surface's tree through it (see [testing.md](../../../../docs/kdos/05-developer/testing.md)).

## Flags

- `-D BUILD_TESTING=OFF` — Build none of `autotests/`, which drive a nested KWin and need the
  Appium and Selenium client libraries, which are not ported.

## Notes

The source is a commit snapshot: upstream tags no releases. The version is the commit's date.

`find_pythonmodule()` in the configure checks for the service's Python modules — flask, pyatspi,
lxml and gi — so they are build dependencies as well as run-time ones. The service also imports
numpy and GTK 3's introspection data, which the configure does not check; both are in `depends`.

`selenium-webdriver-at-spi-run`, the Ruby wrapper upstream's CI starts the service with, is
installed and not used: KDOS starts the service in the running session with `python3 -m flask`,
and Ruby is not in `depends`.

The service's click runs a control's `Press` action whenever it lists one, and a Qt Quick check
box answers `Press` without toggling. `testing/atspi/atspi.py` reads the tree directly, toggling
check boxes with `Toggle`, when it writes goldens.

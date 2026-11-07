## About

pyatspi is the Python API to AT-SPI2 that assistive tools and test drivers are written against:
the accessibility tree of every running application, its states and actions, and keyboard and
event synthesis. Since 2.x it is a thin layer over at-spi2-core's Atspi introspection data, through
[python3-gobject](#port-python3-gobject). [selenium-webdriver-at-spi](#port-selenium-webdriver-at-spi)
is written on it.

## Flags

- `-Denable_tests=false` — Build no tests. They need the dbus-python module and a running test
  registry, neither of which this port's `depends` names.

## Notes

The configure looks up `pygobject-3.0.pc`, which [python3-gobject](#port-python3-gobject)
installs; Python's `gi` module alone does not satisfy it.

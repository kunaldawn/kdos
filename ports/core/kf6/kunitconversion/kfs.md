## About

KUnitConversion converts values between units: length, mass, temperature, speed, pressure, energy,
data sizes, currencies and two dozen more categories, with the unit names and symbols a user may
type. Plasma's KRunner and the unit converter widget answer "10 km in miles" through it, and
[kalzium](#port-kalzium) converts its quantities with it.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The unit names a user types are among those
  strings, so units are recognised by their English names and symbols only.

## Notes

Currency rates are not carried in the library. A program that asks for them makes the library
download the European Central Bank's daily reference rates
(`https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml`) over Qt Network into
`~/.local/share/libkunitconversion/currency.xml`, again once the copy is older than the period the
caller gives, and only while a network connection is up. With no copy, currency conversions give
no result; every other category works offline.

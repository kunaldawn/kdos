## About

KCalc is KDE's scientific calculator: a simple, scientific, statistics and numeral-system mode,
binary bit editing, a constants menu and a result history. Its numbers come from its own `knumber`
library over [gmp](#port-gmp), [mpfr](#port-mpfr) and [mpc](#port-mpc), so it computes to an
arbitrary precision rather than in machine doubles. It is the calculator the desktop opens by
default.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`knumber` declares GMP, MPFR and MPC `TYPE REQUIRED`, so the configure stops without any of the
three and no switch is needed.

The desktop entry's icon is the theme name `accessories-calculator`, which
[breeze-icons](#port-breeze-icons) draws. Upstream's `INSTALL_ICONS` stays at its default, off:
it would install KCalc's own PNGs into `hicolor` under that generic name.

The install links the desktop entry into `/usr/share/kglobalaccel`, which is how the keyboard's
Calculator key starts KCalc. That link is read by kglobalacceld, a daemon of the Plasma session,
not by anything this port installs.

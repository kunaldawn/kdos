## About
foomatic-db-engine reads the [foomatic-db](#port-foomatic-db) printer and driver database and
generates CUPS PPD files from it: `foomatic-ppdfile` for one printer, and
`/usr/lib/cups/driver/foomatic`, a link to `foomatic-ppdfile` that [CUPS](#port-cups)'s
`cups-driverd` runs to list and generate the PPD of every printer in the database. Its Perl
modules, `Foomatic::DB`, `Foomatic::PPD`, `Foomatic::UIElem` and `Foomatic::Defaults`, are
installed under `vendor_perl`, where the system Perl finds them.

## Notes
`make install` creates the `/usr/lib/cups/driver/foomatic` link only when `LIB_CUPS`, which
`build.sh` sets to `/usr/lib/cups`, exists on the building system rather than under `DESTDIR`; the
[cups](#port-cups) dependency is what puts it there. Without it the package carries no driver link
and CUPS lists no foomatic printers.

`build.sh` loads `Foomatic::PPD` from the package after the install, so a module that the
installed Perl cannot compile stops the build instead of failing in the first script that
requires it.

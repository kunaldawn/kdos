## About
foomatic-db-engine reads the [foomatic-db](#port-foomatic-db) printer and driver database and
generates CUPS PPD files from it: `foomatic-ppdfile` for one printer, and
`/usr/lib/cups/driver/foomatic`, which [CUPS](#port-cups) runs to list and generate the PPD of
every printer in the database. Its Perl modules, `Foomatic::DB`, `Foomatic::PPD`,
`Foomatic::UIElem` and `Foomatic::Defaults`, are installed under `vendor_perl`, where the system
Perl finds them.

## Notes
`build.sh` loads `Foomatic::PPD` from the package after the install, so a module that the
installed Perl cannot compile stops the build instead of failing in the first script that
requires it.

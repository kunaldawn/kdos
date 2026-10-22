## About

GPS Manager is a Tcl/Tk program that transfers waypoints, routes and tracks to and from Garmin,
Lowrance and Magellan receivers, and edits, converts and displays them. The second source,
gpsmanshp, is its loadable Tcl extension for reading and writing ESRI Shapefiles through
[shapelib](#port-shapelib). [xastir](#port-xastir) also uses GPSMan to download from a GPS.

## Patches

- `gpsman-system-paths.patch` — In `gpsman.tcl`, set `SRCDIR` to `/usr/share/gpsman/gmsrc`, where `build.sh` installs the program's Tcl sources, instead of the relative `gmsrc`, a path resolved against the directory GPSMan is started from; and set the default manual address to empty, because the HTML manual is not installed.
- `gpsmanshp-tcl9.patch` — Port gpsmanshp to the Tcl 9 API: every command's `Tcl_Obj *CONST objv[]` becomes `Tcl_Obj *const objv[]` (Tcl 9 drops the `CONST` macro), and `GSHPCreateTR` reads the list length from `Tcl_ListObjGetElements` into a `Tcl_Size`, then converts it to the `int` it uses.

## Notes

Neither source has a build system that fits a package. GPSMan is a script, so `build.sh` installs
`gpsman.tcl` as `/usr/bin/gpsman` and copies `gmsrc/` beside it under `/usr/share/gpsman`.
gpsmanshp is compiled by hand: one C file, linked as a shared object against `-lshp` and
`-ltcl9.0`, installed as `/usr/lib/gpsmanshp<version>/gpsmanshp.so` with a `pkgIndex.tcl` that
`build.sh` writes. The `tclsh9.0` line then runs `package require gpsmanshp` against the package
directory and fails the build unless the extension loads and reports its own version.

The menu icon is made from the GIF logo in the HTML manual: upstream's logo exists only as GIF,
which the panel does not read, so ImageMagick pads it square and scales it to hicolor PNGs. `StartupWMClass=Gpsman` is the Tk class, the script's name with its first
letter capitalised.

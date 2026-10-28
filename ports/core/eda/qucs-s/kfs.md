## About

Qucs-S is a circuit simulator with a schematic editor in front of it: draw a circuit, choose an
analysis, and plot the result. The simulation itself is done by [ngspice](#port-ngspice), found on
`PATH` at run time, or by `qucsator_rf`, the RF simulator built from the same archive. The
package also carries the filter, active-filter, attenuator, transmission-line, power-combiner and
RxCalc calculators and `qucs-sspar-viewer`, an S-parameter viewer.

## Flags

- `-DWITH_ADMS=OFF` — Build `qucsator_rf` without the Verilog-A device models, which are compiled
  through `admsXml`. ADMS is not a port, and with the option on configure stops on
  `admsXml required in PATH`. Unset, it is off.
- `-DUPDATE_TRANSLATIONS=OFF` — Compile the shipped `.ts` catalogues as they are, rather than
  regenerate them from the sources. It is upstream's default.

## Patches

- `gperfapphash-no-dos2unix.patch` — In `qucsator_rf/src/CMakeLists.txt`, drops the `dos2unix` run on the generated `gperfapphash.gph`. `gperfappgen` writes the file with `\n` line endings already, and no port provides `dos2unix`, so the custom command fails and stops the build without the patch.

## Notes

The source is upstream's release archive, not a git archive of the tag. The release archive
carries the `qucsator_rf`, `rxcalc` and `qucs-s-spar-viewer` submodules; without them configure
prints "submodule not found" and builds on with the RF simulator and both tools left out. The
`test -x` lines at the end of `build.sh` fail the build when `qucsator_rf` or the viewer is
missing.

The compiled catalogues land in `/usr/share/qucs-s/lang`, which `build.sh` removes; the interface
falls back to its English source strings. The prune that keeps every package English-only is
described in
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).

Qt takes the Wayland app_id from the executable's name, so the S-parameter viewer's entry, which
upstream does not install, gives `StartupWMClass=qucs-sspar-viewer`.

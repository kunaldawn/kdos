# Licences

This appendix lists every licence the ports of KDOS are distributed under, by its SPDX identifier,
with every port under each. It is generated from the recipes each time the book is rendered, so it
covers every recipe in the tree, including those no phase builds.

## Where a licence is recorded

Every recipe states its licence in its `license =` key, as an
[SPDX licence expression](https://spdx.github.io/spdx-spec/v2.3/SPDX-license-expressions/): one
or more identifiers from the [SPDX licence list](https://spdx.org/licenses/), joined by `AND`
when the package holds code under each, and by `OR` when it may be taken under either. An
identifier that begins `LicenseRef-` names a licence the SPDX list does not have. `WITH` attaches an
exception, a grant that relaxes the licence before it, as the GCC runtime library exception does
for the runtime code GCC links into the programs it compiles. The expression is what each package page
shows in its *Licence* row, and what `kpkg info` prints as `License:`.

The texts themselves are installed with the packages. kpkg copies every file at the top of the
source whose name starts with `LICENSE`, `LICENCE`, `COPYING`, `COPYRIGHT` or `NOTICE`, in any
case, into `/usr/share/licenses/<name>/`, which the size policy never prunes; a licence kept
elsewhere in the source is installed there by the port's `build.sh`. See
[What kpkg does around build.sh](../../kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh).

## Every licence

An expression is split into its identifiers, so a port under `MIT OR Apache-2.0` is listed under
both, and a port is counted once under an identifier however often its expression names it. The
licences come first, most-used first, and the exceptions after them.

<!-- kfs:licences -->

## See also

- [kpkgbuild](../../kdos/05-developer/writing-ports.md#keys-the-package-manager-reads): the `license =` key
- [Reading a package page](../00-front/reading-a-package-page.md#the-header): where a page shows its licence
- [What is built from source](../../kdos/05-developer/writing-ports.md#what-is-built-from-source): the few things that are not

<!-- book-nav -->
---

*Part XVII. Appendices, chapter 425.* Previous: [424. Build times](sbu.md) · [Contents](../README.md) · Next: [426. Glossary](glossary.md)

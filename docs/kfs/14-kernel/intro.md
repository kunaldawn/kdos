# The kernel

<!-- kfs:phase 60_kernel -->
<!-- kfs:order configuring building installing -->

This phase builds the Linux kernel and its modules for the target root, from KDOS's own
configuration. It installs [dwarves](#port-dwarves), for the `pahole` tool that turns the
kernel's debug information into [BTF]{.idx}, and then [linux](#port-linux) itself. It is the last
phase that builds ports; `70_image` follows and packs the tree into the medium.

The kernel's recipe has more to it than a package page can hold, so it is explained in three
chapters before the package pages. [Configuring the kernel](configuring.md) covers `kdos.config`, how the build
resolves it and refuses a configuration that came out different, and the options the rest of KDOS
needs. [Building the kernel](building.md) covers the compile: the pinned build identity, the
module-signing key, the panic-screen logo and what Rust and BTF need.
[Installing the kernel](installing.md) follows `postinstall.sh` from the module index to the ESP.

Its packages follow in chapters by [shelf]{.idx}, in the shelves' own order, and within a shelf in the
order kpkgdepends builds them. Each port has a page or, when it has nothing unusual about it, a
compact card.

## See also

- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [The build system](../../kdos/05-developer/build-system.md): phases and their package lists

<!-- book-nav -->
---

*Part XIV. The kernel.* Previous: [Part XIII. The desktop](../13-desktop/intro.md) · [Contents](../README.md) · Next: [336. Configuring the kernel](configuring.md)

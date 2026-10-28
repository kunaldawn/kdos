# Preparing

<!-- kfs:order host-container sources fs-overlay build-tree kdosbuild -->

Nothing that goes into KDOS is compiled on the host directly. Every phase runs inside one container
image, `os-dev`, built from the repository's own `Dockerfile`, and that container has no network,
so every upstream source is fetched beforehand, checked against its recipe and kept by hash. This part sets
that up: the container and why it is the only supported host, the sources and the archive that
keeps them, the `fs/` overlay that becomes the root's first files, the layout of `build/`, and
`kdosbuild`, the program that runs every phase in order.

By the end of the part you have a clone, every source fetched and verified, and a command that
builds the whole system. The parts after it follow that command phase by phase.

## See also

- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md): the same journey told once, end to end
- [Developing](../../kdos/05-developer/developing.md): build targets and the narrow iteration loops
- [Part II](../02-cross/intro.md): the first phase

<!-- book-nav -->
---

*Part I. Preparing.* Previous: [Reading a package page](../00-front/reading-a-package-page.md) · [Contents](../README.md) · Next: [1. The build host and the os-dev container](host-container.md)

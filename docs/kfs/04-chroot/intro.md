# The chroot and kpkg

<!-- kfs:order entering kpkg-install -->

From `20_selfhost` on, every package is built inside `build/fs` as a chroot, by kpkg, from a recipe
under `ports/`, or under `src/` for KDOS's own programs. This part shows how the chroot is entered,
with what mounts and what environment, and then follows a single `kpkg install` through every step
it takes. The package pages in the rest of the book describe each of those steps for one port; this
part is where the steps themselves are explained once.

## See also

- [Packaging](../../kdos/03-architecture/packaging.md): ports, packages, the solver and the recipe hash
- [Writing ports](../../kdos/05-developer/writing-ports.md): the recipe format
- [Part V](../05-selfhost/intro.md): the first package phase

<!-- book-nav -->
---

*Part IV. The chroot and kpkg.* Previous: [30. What the bootstrap leaves](../03-bootstrap/what-you-have.md) · [Contents](../README.md) · Next: [31. Entering the chroot](entering.md)

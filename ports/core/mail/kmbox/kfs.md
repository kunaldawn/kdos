## About

kmbox is the KDE PIM library that reads and writes mbox mail folders, built on
[kmime](#port-kmime). [mimetreeparser](#port-mimetreeparser), the Akonadi mbox resource and the
mail importers use it.

## Flags

- `-D USE_UNITY_CMAKE_SUPPORT=OFF` — Compiles each source file on its own instead of in CMake
  unity batches. Upstream's default, named.

## Notes

The tarball has no translations, so `build.sh` passes no translation switch.

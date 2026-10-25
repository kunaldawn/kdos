## About
musl's source carries the C library and nothing else, so three commands that glibc's own package
installs have no source in it: `getconf`, which prints system configuration values such as
`_NPROCESSORS_ONLN` or `PAGESIZE`; `getent`, which looks entries up in `passwd`, `group`, `shadow`,
`hosts`, `services`, `protocols`, `networks` and `ethers`; and `iconv`, which converts text between
character encodings. This port builds the three from the C files Alpine Linux keeps beside its
musl package, pinned by the aports revision in the helper `_aports_rev`. `getconf` and `getent`
come from NetBSD, `iconv` from musl's author.

They answer for musl itself, not for glibc: `getconf` reports musl's limits, `getent` sees what musl's
lookup functions see, and `iconv` offers exactly the conversions musl's `iconv_open` knows
([The base userland](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#the-base-userland)).
Build systems call these programs (the [sysvinit](#port-sysvinit) Makefile, for one, runs
`getconf`), so the port is in the first run of `30_foundation`, with the rest of the userland every
`build.sh` runs in.

## Notes
There is no upstream build system. `build.sh` compiles each file to a program of the same name
with the chroot's `$CC`, `$CFLAGS` and `$LDFLAGS`, so the three get the same
[release flags](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-release-flags)
as every other port, and installs them in `/usr/bin`. The sources are three single files rather
than a tarball, so kpkg copies each into the work directory as it is.

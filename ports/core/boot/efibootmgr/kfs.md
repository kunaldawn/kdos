## About

efibootmgr reads and changes the UEFI firmware's boot entries, the `Boot####` variables and
`BootOrder` kept in NVRAM, through the kernel's `efivarfs`. `kinstall` runs it once, as
`efibootmgr --create --disk … --part … --loader … --label KDOS`, to write the `KDOS` entry for the
loader the firmware can run; see
[The NVRAM boot entry](../../../../docs/kdos/04-programs/kinstall.md#the-nvram-boot-entry).
It is installed on every system, not only on the installer's medium: a firmware that later
reorders or drops the entry leaves a KDOS install that does not boot, and without a tool on the
running system the only repair is the firmware's own menu. It reads and writes the variables with
the libefivar and libefiboot libraries of [efivar](#port-efivar).

## Flags

- `EFIDIR=kdos` — The directory under `\EFI\` on the EFI system partition that this distribution
  reserves, compiled into `efibootmgr` and written into its manual page as the default loader,
  `\EFI\kdos\grub.efi`, used when `--create` is given no `--loader`. The Makefile refuses to
  build until it is set, either here or as `efibootmgr.efidir` in the builder's `git config`.
  `kinstall` always passes `--loader`, so the default is never used by KDOS itself.
- `libdir=/usr/lib` — Upstream's `Make.defaults` defaults `libdir` to `$(prefix)/lib64`, which on
  KDOS is only a link to `/usr/lib`. efibootmgr installs nothing under it, and none of its
  templates contains the `@@LIBDIR@@` the value is substituted for, so it changes no output.
- `bindir=/usr/sbin`, `mandir=/usr/share/man` — The install target puts both programs,
  `efibootmgr` and `efibootdump`, in `$(sbindir)`, which defaults to `/usr/sbin`, and the manual
  pages in `$(mandir)`, which defaults to `/usr/share/man`; neither setting moves a file.
- `-Wno-error`, `-Wno-address`, `-Wno-stringop-overflow` — Added to `CFLAGS`. Upstream's compile
  line puts its own `-Werror` after `$(CFLAGS)`, and GCC takes whichever of `-Werror` and
  `-Wno-error` comes last, so `-Wno-error` is overridden there. The two named warnings are the ones
  turned off outright, so `-Werror` cannot turn them into errors.

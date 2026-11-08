## About

KDNSSD is the KDE Frameworks library for DNS-SD service discovery over multicast DNS (Zeroconf):
it browses for services such as printers, file shares and game servers on the local network, and
publishes a program's own. It speaks to `avahi-daemon` over the system bus. The network games
built on [libkdegames](#port-libkdegames) find their opponents through it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Avahi=ON` — Make Avahi a requirement. Upstream looks for it as
  optional, falls back to a plain `dns_sd.h` implementation when it is missing, and with neither
  builds a library whose every lookup finds nothing, with only a line in the configure summary.
  The configure reads only a header of [avahi](#port-avahi), `avahi-common/defs.h`; the library
  talks to the daemon over D-Bus and links no Avahi library.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

[avahi](#port-avahi) is also a run-time dependency: without `avahi-daemon` running, browsing finds
no service.

## About

KWalletManager is the front end to KDE's wallet: it lists the wallets, opens one to browse, add,
edit and delete its folders and entries, imports and exports wallets, and shows a tray icon while a
wallet is open. It also installs the wallet page of System Settings (`kcm_kwallet5`). Everything it
shows comes over D-Bus from the wallet daemon in [kwallet](#port-kwallet). The program is installed
as `kwalletmanager5`, the name upstream keeps.

## Flags

- `-D BUILD_DOC=ON` — Build the handbook through [kdoctools](#port-kdoctools), which is then a
  required package. On by default upstream; named so the handbook cannot drop out.
- `-D ENABLE_KAUTH=ON` — Build the settings page's KAuth helper and install its polkit action,
  `org.kde.kcontrol.kcmkwallet5.save`, through [kauth](#port-kauth). On by default upstream and
  off only for Flatpak; named so it stays on.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The KAuth helper does no work of its own: the settings page runs it so that polkit asks for the
person's own password (`auth_self`) before the wallet settings are saved. The question is asked
by a polkit authentication agent, which the Plasma session runs; with none, saving is refused.
`kwalletmanager5-kwalletd.desktop` is the hidden entry the wallet daemon uses to open the manager
on a wallet, and is `NoDisplay=true` upstream.

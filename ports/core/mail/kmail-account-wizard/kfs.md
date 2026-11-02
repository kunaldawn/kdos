## About

The account wizard sets up a mail account for [kmail](#port-kmail) from a name, an address and a
password. It looks the address's domain up in Mozilla's ISPDB and in the domain's own autoconfig
files, offers the IMAP or POP3 and SMTP settings it finds (or takes them by hand), and then creates
the Akonadi resource, the identity through [kidentitymanagement](#port-kidentitymanagement) and the
SMTP transport through [kmailtransport](#port-kmailtransport). KMail runs it, as `accountwizard`,
on its first start and from its settings; [kontact](#port-kontact) runs it too. Its window is a
Kirigami QML interface.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

The configure does not check the QML modules the interface imports at run time:
`org.kde.kirigami` from [kirigami](#port-kirigami), `org.kde.kirigamiaddons.formcard` and
`.components` from [kirigami-addons](#port-kirigami-addons), and the `org.kde.desktop` style from
[qqc2-desktop-style](#port-qqc2-desktop-style), which `main.cpp` selects unless
`QT_QUICK_CONTROLS_STYLE` names another. All three are in `depends` for run time; without
kirigami or kirigami-addons the window does not load. [kdepim-runtime](#port-kdepim-runtime) is in `depends` because the
`akonadi_imap_resource` and `akonadi_pop3_resource` the wizard creates are its programs.

The lookup reads `config-v1.1.xml` from `https://autoconfig.<domain>/mail/`,
`http://autoconfig.<domain>/mail/` and `https://<domain>/.well-known/autoconfig/mail/`, and the
domain's entry from Mozilla's database at `https://autoconfig.thunderbird.net/v1.1/`. It runs only
inside the wizard, for the address the user typed.

The configure finds [qtkeychain](#port-qtkeychain) as an optional package, but no target links
it, so it is not in `depends`; the account password is stored by the resource the wizard creates.

The one entry upstream installs, `org.kde.accountwizard.desktop`, is `NoDisplay=true`: the wizard
is reached from KMail and Kontact, not from the menu.

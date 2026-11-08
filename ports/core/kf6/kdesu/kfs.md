## About

KDESu runs a program as another user, root by default, from a KDE program: it drives `sudo` (or
`su`, or `doas`) on a pseudo-terminal from [kpty](#port-kpty), answers its password prompt with
the password the user typed into a dialog, and starts the program through `kdesu_stub`, which
passes on the environment the program needs. The `kdesu` command of
[kde-cli-tools](#port-kde-cli-tools) is its front end.

## Flags

- `-D KDESU_USE_SUDO_DEFAULT=ON` — Use `sudo` as the default backend, which asks for the user's
  own password. The installer locks the root account by default and makes the first user a member
  of `wheel`, which [sudo](#port-sudo)'s policy lets run anything (see
  [The pages](../../../../docs/kdos/04-programs/kinstall.md#the-pages)), so upstream's default,
  `su`, would ask for a root password that does not exist. sudo is in `depends` for it.
- `-D KDESU_USE_DOAS_DEFAULT=OFF` — Do not make `doas` the default; upstream refuses a configure
  that asks for both, and this states which one the recipe chose.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The configure looks for libX11 as optional and, finding it, lets `kdesu_stub` pass the X
authority on to the program it starts, for a program that runs under Xwayland;
[libX11](#port-libX11) is in `depends` so that this is always built.

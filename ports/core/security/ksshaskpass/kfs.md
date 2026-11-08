## About

ksshaskpass is the passphrase prompt OpenSSH runs when a program asks for a key's passphrase with
no terminal to ask on: `ssh-add` started at login, `ssh` or `git` started from a launcher. It shows
a Qt dialog, answers the prompt on its standard output, and can keep the passphrase in the
session's secret store through [qtkeychain](#port-qtkeychain), so the next prompt for the same key
is answered without asking.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The manual page `ksshaskpass(1)`, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional and skips without a word.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

OpenSSH runs the program named by `SSH_ASKPASS` when it has no terminal to ask on and either
`DISPLAY` or `WAYLAND_DISPLAY` is set; the Plasma session sets both, so a key added from a
launcher or an autostart entry is asked for in this dialog. With `SSH_ASKPASS_REQUIRE=prefer`
OpenSSH asks through the dialog even when it has a terminal, and with `force` even when no
display is set. Git runs the same program for its own prompts when `GIT_ASKPASS` and
`core.askPass` are unset. The Plasma session's environment script,
`/usr/share/kdos/xdg/plasma-workspace/env/kdos.sh`, sets `SSH_ASKPASS=/usr/bin/ksshaskpass` when
nothing has set it, and leaves `SSH_ASKPASS_REQUIRE` unset, so `ssh` in a terminal still asks
there.

The install puts a hidden desktop entry, `org.kde.ksshaskpass.desktop`, in
`/usr/share/applications`: it gives the prompt's window its name and icon and is not a launcher.

qtkeychain keeps a passphrase in the session's secret store, which in the Plasma session is
`ksecretd` from [kwallet](#port-kwallet).

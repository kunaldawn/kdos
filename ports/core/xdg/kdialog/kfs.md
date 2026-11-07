## About

KDialog puts KDE dialogs in shell scripts: message and yes/no boxes, text, password and
combo-box prompts, check and radio lists, file and folder pickers, a colour picker, a calendar,
passive popups and a progress bar that the script drives over D-Bus. It prints what the person
chose on standard output and answers with its exit status. Browsers and scripts that run under
KDE call it for their file pickers.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON` — Make [libX11](#port-libX11) a requirement. KDialog
  uses it for one thing, parsing an X geometry string with `XParseGeometry` for `--geometry`;
  without it the option is accepted and ignored. Under Wayland the size the string gives still
  applies, and the position does not, since a Wayland client cannot place its own window.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`--attach` makes a dialog the child of another window through KWindowSystem. Under Wayland the
value is an exported xdg-foreign handle, not an X window id, so only a caller that exports its
window can attach to it.

The desktop entry is `NoDisplay=true` upstream: KDialog is started by scripts, never from the
menu.

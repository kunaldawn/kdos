## About

KTextEditor is KDE's text editor component, the editing part of [kate](#port-kate) and
[kdevelop](#port-kdevelop): syntax highlighting through
[syntax-highlighting](#port-syntax-highlighting), code folding, multiple cursors, the vi input
mode, spell checking through [sonnet](#port-sonnet), search and replace, printing and a
JavaScript scripting interface over Qt Qml. It is both a library and a [kparts](#port-kparts)
part, so [rkward](#port-rkward), [cantor](#port-cantor) and other programs embed the same editor.

## Flags

- `-D ENABLE_KAUTH=ON` — Save a file the user may not write through [kauth](#port-kauth): the
  editor asks polkit for the right, and a helper writes the file as root. It is upstream's default
  on Linux; on, the configure requires KF6Auth.
- `-D CMAKE_DISABLE_FIND_PACKAGE_EditorConfig=ON` — Never look for the EditorConfig C library,
  with which the editor would apply a project's `.editorconfig` settings (indentation, line
  endings) to the files it opens. It is not a port, so the switch keeps the result from depending
  on what is installed in the build root.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The configure requires Qt Text to Speech, for reading the selection aloud, so
[qt6-qtspeech](#port-qt6-qtspeech) is in `depends` with
[qt6-qtdeclarative](#port-qt6-qtdeclarative) for the scripting interface.

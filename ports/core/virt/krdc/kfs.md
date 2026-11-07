## About

KRDC is KDE's remote desktop client: it connects to another machine over VNC, through
[libvncserver](#port-libvncserver)'s client library, or over RDP, through FreeRDP 3
([freerdp](#port-freerdp)), keeps bookmarks, finds VNC and RDP servers on the local network over
DNS-SD with [kdnssd](#port-kdnssd), and can reach a VNC server through an SSH tunnel. Saved
passwords go to the Secret Service through [qtkeychain](#port-qtkeychain).

## Flags

- `-D WITH_VNC=ON` — With `-D WITH_RDP=ON`: the two protocols. Both are on by default upstream,
  and each then makes its library a requirement; named so neither can be turned off by a default
  that changes.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibSSH=ON` — The SSH tunnel for VNC connections, through
  [libssh](#port-libssh). Upstream looks for it as optional and leaves the tunnel out without
  it; required, a missing library stops the configure.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON` — Wayland support in the viewer: with it
  KRDC asks the compositor through the keyboard-shortcuts-inhibit protocol to pass Alt+Tab and the
  other desktop chords to the remote machine while its window has focus. Upstream finds
  [qt6-qtwayland](#port-qt6-qtwayland) quietly and compiles the code out without it. The protocol
  code is generated at build time with `wayland-scanner`, from [wayland](#port-wayland).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_SharedMimeInfo=ON` — Install `org.kde.krdc-mime.xml`, which
  defines `application/x-krdc` for `*.rdp` connection files. Upstream installs it only when it finds
  [shared-mime-info](#port-shared-mime-info), and the desktop entry claims the type either way;
  without the definition the claim names a type that does not exist and `.rdp` files do not open
  in KRDC.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` is KDOS's own, the change the tree makes to the entries of
[dolphin](#port-dolphin), [ark](#port-ark) and the other KDE applications whose class is not their
app_id. Plasma's task manager matches a window to its entry by the app_id as well as
by `StartupWMClass`, so the launcher and the running window agree either way; the class matters to
anything that reads `StartupWMClass` alone.

## Notes

Network discovery needs avahi-daemon running; without it the browser lists nothing and
connections by address still work. The entry claims `x-scheme-handler/vnc` and
`x-scheme-handler/rdp`, which Remmina claims too; the desktop's `mimeapps.list` decides which one
opens a link.

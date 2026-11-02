## About

KRuler is an on-screen ruler: a translucent strip, horizontal or vertical, that measures distances
in pixels and shows the colour under its marker, for checking the layout of a dialog or a page. It
lives in the system tray between uses, through
[kstatusnotifieritem](#port-kstatusnotifieritem).

## Flags

- `-D WITHOUT_X11=ON` — Leave out the XCB code. It runs only when KRuler is on the X11 platform
  plugin, where it marks the window as an override window and adds the "native moving" and "always
  on top" settings page; KRuler on KDOS is a Wayland client, so the code would never run. Upstream
  finds XCB quietly and compiles it in when the headers happen to be present.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

On Wayland a client cannot place its own window or keep it above others, so KWin decides where the
ruler goes and the ruler is moved by dragging it like any window.

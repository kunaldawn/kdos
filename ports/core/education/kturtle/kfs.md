## About

KTurtle is KDE's educational programming environment: programs written in TurtleScript, a
Logo-like language whose commands can be typed in the learner's language, steer a turtle that
draws on a canvas, with the code highlighted as it runs.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Requires [kdoctools](#port-kdoctools) for the
  handbook, which upstream builds only when it finds kdoctools.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.kturtle`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.

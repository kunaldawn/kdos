## About

Minuet is KDE Education's ear trainer: exercises on intervals, chords, scales and rhythm, played
on a piano keyboard and staff, which the learner answers by ear. Sound is synthesised by
[fluidsynth](#port-fluidsynth) from a General MIDI soundfont the package carries, and the singing
exercises listen through the microphone and detect the pitch with [aubio](#port-aubio).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`skip-po-processing.patch` is upstream's commit `b7a4b6a1` in `education/minuet`, which follows
this release. With `KF_SKIP_PO_PROCESSING` on, `ki18n_install()` returns before it creates the
`pofiles` target, and the release's unconditional `add_dependencies(minuet pofiles)` stops the
configure at generation with `The dependency target "pofiles" of target "minuet" does not
exist`. The patch adds the dependency only when the target exists. A release that contains the
commit makes the patch fail to apply, and it is then dropped.

## Notes

The configure asks pkg-config for `fluidsynth >= 2.5.3`, which needs [pkgconf](#port-pkgconf).
On the desktop it also requires the QML modules it loads, `org.kde.kirigami`,
`org.kde.coreaddons`, the kirigami-addons `formcard` and `onboarding` modules and
`org.kde.desktop`, through `ecm_find_qmlmodule(... REQUIRED)`, so
[qqc2-desktop-style](#port-qqc2-desktop-style) is a build dependency here, not only a run-time
one.

The soundfont is GeneralUser GS 1.47 by S. Christian Collins, about 31 MB, installed with the
FluidSynth plugin; its licence is its author's own, named `LicenseRef-GeneralUser-GS` in the
recipe, and the tarball carries no copy of the text. The microphone exercises use Qt Multimedia's
capture and so need a capture device in the session. The sound and microphone back ends are
plugins Minuet loads from its own plugin directory.

`KAboutData` sets the desktop file name `org.kde.minuet`, the name of upstream's entry, so the
Wayland app_id matches it.

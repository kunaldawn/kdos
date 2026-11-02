## About

Step is KDE Education's interactive physics simulator: the learner places bodies, springs,
motors, gases and charged particles on a canvas, starts the clock and watches the simulation,
with graphs and meters of any quantity. Its engine, StepCore, solves the motion with
[eigen](#port-eigen) and, optionally, the GSL integrators; units in the property editor convert
through [libqalculate](#port-libqalculate).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream looks for it as an optional package, and
  without it leaves the handbook out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_GSL=ON` — Stop the configure if [gsl](#port-gsl) is missing.
  Upstream marks it optional and sets `STEPCORE_WITH_GSL` from the result of the find, so without
  it the GSL-powered solvers silently disappear from the solver list.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qalculate=ON` — Stop the configure if
  [libqalculate](#port-libqalculate) is missing, for the same reason: `STEP_WITH_QALCULATE`
  follows the find, and without it property values cannot be typed with units.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Python3=ON` — Do not look for Python. Upstream uses it only to
  write a translated copy of every example, tutorial and object description for each language in
  `po/`, compiled from that language's catalogue by `step/data/extractxml`. Those copies are
  translations in all but their file type, so the language rule would not remove them, and this
  tree ships English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  Without Python the English examples, tutorials and object descriptions are still installed.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Step finds Eigen through `Eigen3Config.cmake`. The tree's [eigen](#port-eigen) is the 5.0 series,
which still installs its CMake package under that name.

Upstream installs its own MIME package, `org.kde.step.xml`, which defines `application/x-step`
for the entry's `MimeType=`; `kpkg` rebuilds the MIME database on install, and
[shared-mime-info](#port-shared-mime-info) is in `depends` because the configure requires it.
More examples come through [knewstuff](#port-knewstuff). `KAboutData` gives the window the app_id
`org.kde.step`, the name of upstream's entry.

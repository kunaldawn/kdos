## About

dolphin-plugins extends Dolphin. The Git plugin shows each file's state as an overlay and puts
commit, push, pull, checkout and log in the context menu; the Makefile action runs a target from
the context menu of a `Makefile`; the ISO action mounts and unmounts a disc image, and an overlay
marks an image that is mounted.

## Flags

- `-D BUILD_svn=OFF` — With `-D BUILD_hg=OFF` and `-D BUILD_bazaar=OFF`: no Subversion, Mercurial
  or Bazaar plugin. Each one runs its tool's command line (`svn`, `hg`, `bzr`) for every folder it
  marks, and none of the three is a port, so the plugins would activate in a checkout and fail on
  every action. Subversion and Mercurial are deferred ports; when one lands, its switch is turned on
  with the tool in `depends`.
- `-D BUILD_dropbox=OFF` — No Dropbox plugin. It talks to the proprietary Dropbox client's socket
  in `~/.dropbox`, and that client is not something this tree can build.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The version-control plugins build against `DolphinVcs`, the plugin interface library
[dolphin](#port-dolphin) installs, which the configure requires. Depending on dolphin makes it a
build dependency of another port, so dolphin is listed in `43_toolkits` rather than `44_apps`.

The Git plugin runs [git](#port-git), which is in `depends` for run time only. The ISO action
asks udisks2 over D-Bus to set up a loop device (`LoopSetup`) and mount it; that runs without a
password in an active elogind session under [udisks2](#port-udisks2)'s polkit defaults.

Each switch is a `BUILD_<directory>` option that ECM's `ecm_optional_add_subdirectory()` creates;
the subdirectories left at their default, on, are `git`, `makefileactions`, `mountiso` and
`mountedisooverlay`.

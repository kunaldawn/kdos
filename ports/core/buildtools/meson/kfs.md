## About

Meson is the build system a large part of the tree is configured with: it reads a project's
`meson.build` and writes the build files [ninja](#port-ninja) runs. It is installed in the first,
ordered run of `30_foundation`, the build tools every later recipe reaches for without naming them
in `depends`. Before it, `muon`, a meson written in C, builds `pkgconf`, because muon needs nothing
but a C compiler; see the [muon](#port-muon) page.

Every meson setup in the chroot goes through `script/bin/meson`, which adds
`--wrap-mode=nodownload` to a setup that chooses no wrap mode, so no build can fetch a subproject
from the network. The wrapper answers as a missing command until this port is installed; see
[The phase environment](../../../../docs/kdos/05-developer/build-system.md#the-phase-environment).

Meson is a pure Python package with a setuptools backend and no run-time dependency of its own.
The port installs it with pip, from the unpacked source.

## Flags

- `--no-deps` — Install meson alone and resolve no requirements. Meson declares no run-time
  requirement, only optional extras (`ninja`, `progress`, `typing`) that pip installs only when
  one is asked for by name.
- `--no-index` — Never consult the Python package index. The build has no network, and the source
  directory is the only thing to install.
- `--no-build-isolation` — Build the wheel with the setuptools already installed, from the
  `python3-setuptools` port in `depends`, instead of fetching setuptools into a private build
  environment.
- `--ignore-installed` — Write the files into `$PKG` without first uninstalling a meson pip finds
  in the build root's own `site-packages`, which a rebuild of this port always finds. Every
  `pip install --root` in the tree passes it; see
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- `--root=$PKG` — Install under the package directory, with `--prefix=/usr` putting the module in
  `/usr/lib/python3.*/site-packages` and the `meson` script in `/usr/bin` beneath it.

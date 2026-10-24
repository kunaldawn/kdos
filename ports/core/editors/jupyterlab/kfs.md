## About

JupyterLab and the Jupyter Notebook are computational notebooks: a local server, started with
`jupyter-lab` or `jupyter-notebook`, runs code cells through a kernel such as
[python3-ipykernel](#port-python3-ipykernel) and serves the documents to the web browser. The
port installs both front ends and the server packages they share, from one vendor bundle.

## Flags

- `HATCH_JUPYTER_BUILDER_SKIP_NPM=1` — Keep every `hatch-jupyter-builder` build hook from running
  npm. The release archives carry their web front ends already built (`jupyterlab/static`,
  `notebook/static`, `jupyter_server/static`), and the build has no network. A hook's
  `ensured-targets` are still checked, so a missing built file fails the build.
- `ARGON2_CFFI_USE_SYSTEM=1` — Build `argon2-cffi-bindings` against the [argon2](#port-argon2)
  port instead of compiling the copy of libargon2 in its source.

## Notes

`vendoring = python` with an explicit `pypackages` list: the bundle holds the source
distributions of every Python package the two front ends need that is not a port of its own,
plus the build requirements of each, and `build.sh` installs that list `--no-deps` from it (see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)). A
package listed here is owned by this port, so a module that another program also imports belongs
in a `python3-*` port that both name in `depends`, never in this bundle.

Two build backends in the bundle, `hatch-fancy-pypi-readme` and `pdm-backend`, are not ports.
They go into `$SRC_ROOT/backends`, which is put on `PYTHONPATH` for this build only, so nothing
outside the package is written.

pip writes the server's configuration under `/usr/etc/jupyter`, which `build.sh` moves to
`/etc/jupyter`. Beside it, `jupyter_server_config.d/kdos-offline.json` sets three defaults that
otherwise reach the network: the extension manager is `readonly` instead of installing from PyPI,
`news_url` is null so no announcements feed is fetched, and the update check is
`jupyterlab.NeverCheckForUpdate`.

Upstream ships both icons only as SVG, which `build.sh` rasterises to PNG for the panel. Both
upstream menu entries start a server in a terminal and open it in the web browser. The Notebook
entry is replaced with one that claims no file types, so only JupyterLab opens notebook files.

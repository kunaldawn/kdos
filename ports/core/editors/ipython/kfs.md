## About

IPython is an interactive Python shell: completion as you type, syntax highlighting, a history kept
across sessions, `?` and `??` for an object's documentation and source, and the `%` magic commands
for timing, running files and calling the shell. Jupyter's Python kernel and Spyder's console are
built on it, so [python3-ipykernel](#port-python3-ipykernel), [spyder](#port-spyder),
[jupyterlab](#port-jupyterlab) and several other Python ports depend on it. The port installs
IPython with the pure-Python packages it needs that no other port provides, its manual page, and
a desktop entry, *Python Shell*, that opens it in a terminal.

## Notes

- **The bundle's packages are named one by one.** The vendor bundle holds IPython's whole dependency
  set as source distributions, with the build backends they need. `build.sh` installs with
  `--no-deps` and names each package that no other port provides (`jedi`, `parso`, `prompt-toolkit`,
  `traitlets`, `pexpect` and the rest) plus IPython itself. Pygments, psutil and wcwidth are
  imported by other programs too, so they are ports of their own in `depends`, and the bundle's
  copies of them are never installed. A resolving install would instead package whatever the build
  root happened to lack, and which package owned those three would follow build order. The rule is
  in
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- **Nothing is installed into the build root.** pip builds each source distribution in an isolated
  environment whose backends (setuptools, flit-core, hatchling, poetry-core and their helpers) also
  come from the bundle, through `--no-index --find-links=vendor`, and `--ignore-installed` keeps pip
  from touching the copies already in the build root.
- **A new IPython can need a new name in the list.** When a version adds a dependency, the bundle
  `ports/fetch` makes carries it, but `build.sh` installs only what it names, so the package would
  lack the module and `ipython` would fail on its import.

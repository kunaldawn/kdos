## About
hatch-vcs is the hatchling plugin that sets a project's version from version control, by calling
[python3-setuptools-scm](#port-python3-setuptools-scm). A project lists it beside hatchling in
its build requirements and sets `[tool.hatch.version] source = "vcs"`.

In KDOS, [python3-sphinx](#port-python3-sphinx) needs it for part of its closure, and later
phases build python3-scikit-build-core, python3-urllib3, python3-attrs, python3-jsonschema,
khal, deluge and other ports with it.

## Flags
- `--no-build-isolation` — Build with the backend already installed in the build root rather
  than one pip would fetch into a temporary environment. hatch-vcs builds with hatchling
  (`requires = ["hatchling>=1.1.0"]`), so [python3-hatchling](#port-python3-hatchling) is in
  `depends`; setuptools-scm is there because hatch-vcs imports it when it runs.

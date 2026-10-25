## About
MarkupSafe provides `Markup`, a string type that marks text as safe for HTML and XML, and the
`escape` function that turns any other string into one. Template engines use it to escape
whatever a template inserts unless it is already marked safe.

In KDOS it is the escaping layer of [python3-jinja2](#port-python3-jinja2) and
[python3-mako](#port-python3-mako), and of [python3-werkzeug](#port-python3-werkzeug) and
[python3-flask](#port-python3-flask). The fast path of its escaping is a C
extension, `markupsafe._speedups`, which this port requires.

## Flags
- `CIBUILDWHEEL=1` — Make the C extension mandatory. MarkupSafe's `setup.py` otherwise catches a
  failed compile, prints a warning and builds a pure-Python package instead, which installs and
  imports without complaint; with the variable set, the failure stops the build. See
  [Python modules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).
- `--no-build-isolation` — Build with the setuptools already installed in the build root
  (`requires = ["setuptools>=77"]`) rather than one pip would fetch into a temporary
  environment; [python3-setuptools](#port-python3-setuptools) is in `depends`.

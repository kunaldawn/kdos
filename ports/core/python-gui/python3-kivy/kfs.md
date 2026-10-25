## About

Kivy is a Python framework for touch-friendly graphical applications. It draws everything itself
with OpenGL and gets its window, input, images, sound and fonts from SDL2, which on KDOS is
[sdl2-compat](#port-sdl2-compat) over SDL 3, so a Kivy program runs as a Wayland client of the
compositor. It is an application toolkit; nothing in the desktop links it.

## Flags

Kivy's `setup.py` reads its build options from the environment: each `USE_<NAME>` variable it
finds overrides the matching entry of its `c_options` table. Left unset, SDL2, Pango and
GStreamer are decided by what `pkg-config` finds in the build root, OpenGL ES 2 by the
`KIVY_GRAPHICS` variable, and X11, Wayland, EGL and MesaGL default to off. All eight are set here, so the compiled providers do not follow the
build root.

- `USE_SDL2=1` — Build the SDL2 window, image, audio, text and clipboard providers, the only
  backends this port uses.
- `USE_X11=0` — Skip the native X11 window provider, which would link `libX11` and `libXrender`
  directly. Windows are SDL's.
- `USE_WAYLAND=0` — Compile out the Wayland handles that `Window.get_window_info()` returns for a
  Wayland window (`wl_display`, `wl_surface`, `wl_shell_surface`). The window still runs on
  Wayland through SDL; only that query goes, which also keeps the build off the Wayland headers.
- `USE_EGL=0` — EGL instead of GLX for the X11 provider, which is not built.
- `USE_GSTREAMER=0` — Skip the GStreamer audio and video player. [gstreamer](#port-gstreamer) is a
  port but not in `depends`; sound goes through SDL's mixer.
- `USE_PANGOFT2=0` — Skip the Pango text provider. [pango](#port-pango) is a port but not in
  `depends`; text is rendered through SDL2_ttf.
- `USE_MESAGL=0` — Leave off the header redirection Kivy uses for headless test builds, which
  stubs `glBlendEquationSeparate` and maps the GLES depth calls onto desktop GL ones.
- `USE_OPENGL_ES2=0` — Compile against desktop OpenGL and link `libGL`, from
  [libglvnd](#port-libglvnd), rather than OpenGL ES 2.
- `--no-build-isolation` — Build with the Cython, setuptools and wheel already in the build root.
  Kivy's `pyproject.toml` caps Cython below the release the tree carries and pins setuptools,
  and the C that the capped Cython generates does not follow the C API of the tree's Python. Under this flag pip does not enforce
  those pins, so the [python3-cython](#port-python3-cython) port compiles Kivy, and the two Cython
  patches below are what that newer Cython needs. The pinned backends in the vendor bundle are
  there only because `ports/fetch` collects every declared build requirement; none is installed.

## Patches

- `python314-ast-str.patch` — In `kivy/lang/parser.py`, drops the branch that tests for `ast.Str`, which Python 3.14 does not have, and reads every value of an f-string or boolean expression through `.value`. Without it, parsing any `.kv` rule that contains an f-string or an `and`/`or` raises `AttributeError`.
- `cython-long.patch` — Removes the uses of `long`, Python 2's integer type: `hsv_to_rgb()` in `context_instructions.pyx` casts with `<long>` instead of calling `long()`, the two `isinstance(…, (long, int))` tests in `opengl.pyx` test `int` alone, and the `__long__` method of `weakproxy.pyx` goes.
- `cython-dict-keys.patch` — In `RenderContext.apply()` in `instructions.pyx`, removes the Python 2 branch, which assigned a dictionary's `keys()` to a variable declared `cdef list`, and keeps the line that wraps them in `list()`.

## Notes

The one module Kivy needs that is not a port, `filetype`, comes from the vendor bundle that
`ports/fetch` makes from `pypackages = filetype`; `pip3 install --no-index` installs it and Kivy
from `vendor/` alone. See
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

`build.sh` deletes `/usr/share/kivy-examples`, the example programs Kivy's `setup.py` installs as
data files.

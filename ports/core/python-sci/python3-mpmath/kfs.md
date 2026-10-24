## About
[mpmath]{.idx} is arbitrary-precision floating-point arithmetic for Python, real and complex,
with special functions, numerical integration, root finding and linear algebra at any precision.
[sympy](#port-sympy) evaluates expressions numerically through it. When gmpy2 imports, mpmath
does its integer arithmetic with GMP through it, and [python3-gmpy2](#port-python3-gmpy2) is
in `depends`.

## Notes
The vendor bundle is mpmath's build backend and nothing installed: mpmath builds with setuptools
and setuptools-scm, and the bundle carries them with `vcs-versioning`, `packaging` and
`flit_core`. The install runs with pip's build isolation on, and pip builds that environment
from `vendor` (`--no-index --find-links=vendor`).

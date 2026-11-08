## About

zope.interface declares interfaces in Python: an interface names the methods and attributes an
object promises, a class declares that it implements one, and an adapter registry looks up the
object that provides an interface for another. [python3-twisted](#port-python3-twisted) is built
on it, and through Twisted so is [kajongg](#port-kajongg).

## Notes

The C extension `_zope_interface_coptimizations` speeds up every interface check. `setup.py`
builds it through a `build_ext` that turns a compile failure into a printed warning, and the
package then installs with the pure-Python implementation alone. The last line of `build.sh`
lists the installed extension module, so a failed compile stops the build instead of shipping
the slow fallback.

The package's own `zope/interface/tests` modules install with it; nothing removes them.

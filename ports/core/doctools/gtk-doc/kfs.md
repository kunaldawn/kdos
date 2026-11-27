## About
gtk-doc builds the API reference of a C library from the comments in its sources, as GLib, GTK and
many GNOME libraries write them. A library's build runs its tools when it is configured with its
documentation on.

## Notes
[python3-pygments](#port-python3-pygments) and [python3-lxml](#port-python3-lxml) are in `depends`
for their run time. `gtkdoc-mkhtml` and `gtkdoc-fixxref` highlight the code examples with Pygments,
importing it from `gtkdoc/highlight.py` on every run, and `gtkdoc-mkhtml2` imports lxml's `etree`
when it starts. Without them in `depends`, the one path to Pygments runs through build-only ports
(Doxygen, CMake and Sphinx, behind libxml2) and none reaches lxml, so a prebuilt install would come
without either.

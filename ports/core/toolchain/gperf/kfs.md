## About
GNU gperf generates a perfect hash function from a set of keywords: given the list, it writes C
or C++ code for a table and a lookup function that recognises any word of the set with a single
probe. Programs use it for the fixed vocabularies they parse, such as configuration keys,
element names or command words.

It is a build-time tool in KDOS. [fontconfig](#port-fontconfig), [eudev](#port-eudev),
[libcap](#port-libcap), [libseccomp](#port-libseccomp), [appstream](#port-appstream),
[webkitgtk](#port-webkitgtk), [qt6-qtwebengine](#port-qt6-qtwebengine) and other ports run it
while they build. The
[chromium](#port-chromium) port links `/usr/bin/gperf` into the place under `third_party/` where
Chromium's build looks for its own prebuilt copy, so Chromium's build runs this one.

The port is upstream's default build, with nothing switched on or off.

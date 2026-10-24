## About

GNU FriBidi implements the Unicode Bidirectional Algorithm, which works out the display order of
text that mixes right-to-left scripts such as Arabic and Hebrew with left-to-right ones. It
installs `libfribidi` and the `fribidi` command, which reorders text given on its input. Text
layout links it: [pango](#port-pango), and through it GTK, [libass](#port-libass)'s subtitle
rendering in [ffmpeg](#port-ffmpeg) and the media players, [libraqm](#port-libraqm) and the
programs that name it directly.

The build is meson's canonical shape; `-Dtests=false` leaves out the test suite. Upstream's `docs`
option, on by default, installs a section 3 manual page for each library function.

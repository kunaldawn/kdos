## About
Noto Sans and Noto Sans Mono are the system's default proportional and monospaced faces: the
preference file this recipe writes puts them at the head of fontconfig's `sans-serif` and
`monospace`, so any program that asks for a generic family gets Noto. Its sources cover Latin, Greek
and Cyrillic; [noto-fonts-extra](#port-noto-fonts-extra) and [noto-cjk](#port-noto-cjk) add the
other scripts. The desktop's own chrome names Terminus and does not depend on this preference.

## Notes
The two sources are upstream's release `.zip` files for Noto Sans and Noto Sans Mono, which are
published separately and versioned apart; the Mono release is pinned in the recipe's `_mono`
helper. kpkg copies a `.zip` into `$SRC` without unpacking it, so `build.sh` unzips each into a
directory of its own. Each archive carries several builds of the face (hinted and unhinted TTF,
OTF, variable fonts); the recipe installs the hinted TTFs, in the weights its two `for` loops name.

`56-noto-preferred.conf` must sort before the `57-dejavu-*.conf` files. Noto Sans Mono carries the
box-drawing and block sets but not the diagonal arrows, some dingbats and a few other symbols, and
fontconfig then finds those in DejaVu, the next face down the list.

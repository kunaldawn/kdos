## About
noto-fonts-extra is the Noto Sans faces for the scripts [noto-fonts](#port-noto-fonts) does not
cover: Arabic, Hebrew, Armenian, Georgian, the Indic scripts, Sinhala, Thai, Lao, Khmer, Myanmar and
Ethiopic. Without them a script that no other installed face covers renders as empty boxes.
Chinese, Japanese and Korean are [noto-cjk](#port-noto-cjk)'s.

## Notes
Noto publishes each script from its own repository with its own version, so the recipe's `version`
is a meta-version and each face is pinned in a helper of its own (`_arab`, `_beng` and the rest),
which its `source` and `sha256 =` lines read. `ports/update` looks at notofonts/arabic, whose newest
tag can be a Noto Naskh Arabic release with no Noto Sans Arabic asset, so it may offer an upgrade
this port must not take; bump the helpers by hand.

`build.sh` unzips each archive into a directory named after its family and installs the hinted TTFs
of the weights its list names: Regular, Light, Medium, SemiBold and Bold, except Oriya, whose
release has no Light, Medium or SemiBold.

`56-noto-scripts.conf` sorts between `56-noto-preferred.conf` and the `57-dejavu-*.conf` files, so
these faces come before DejaVu, which also claims Arabic, Hebrew, Armenian, Georgian and Lao. A
face is named in its lists only if it carries no symbol or Latin coverage that would take the place
of the default faces: Gurmukhi and Sinhala are installed but not listed, and no CJK face is ever
named there.

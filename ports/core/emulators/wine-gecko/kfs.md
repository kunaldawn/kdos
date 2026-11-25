## About

Wine Gecko is the HTML engine Wine's MSHTML loads, a build of Gecko for Windows. The port installs
the 32- and 64-bit trees upstream publishes into `/usr/share/wine/gecko`, where Wine finds them
without downloading them on a prefix's first start.

## Notes

- **The licence text is inside `omni.ja`.** The trees carry no licence file of their own; the
  MPL 2.0 and every third-party notice are `chrome/toolkit/content/global/license.html` inside
  the jar, which [unzip](#port-unzip) extracts into `/usr/share/licenses/wine-gecko`. Gecko's
  jar puts its index ahead of the entries, so `unzip` warns and exits 2 although it extracts the
  file whole; `build.sh` accepts 2, and fails unless the extracted page names the Mozilla Public
  License.

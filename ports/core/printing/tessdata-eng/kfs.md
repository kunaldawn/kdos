## About

tessdata-eng is the trained data tesseract needs to read anything: the English model `eng` and
the orientation and script-detection model `osd` from the `tessdata_fast` set, installed under
`/usr/share/tessdata`.

## Notes

The two sources are the raw `.traineddata` files, which carry no licence text. They are published
under the Apache-2.0, and that licence asks for its text to travel with the data, so the last line
of `build.sh` installs the Apache-2.0 text from `/usr/share/licenses/openssl/LICENSE.txt`, which
OpenSSL ships verbatim. OpenSSL is in `30_foundation`, so every tree that builds this port has
that file.

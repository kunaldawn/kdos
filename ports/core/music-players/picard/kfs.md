## About
[MusicBrainz Picard]{.idx} tags and renames music files from the MusicBrainz database: it
identifies a release from file metadata, an AcoustID fingerprint computed with
[chromaprint](#port-chromaprint)'s `fpcalc`, or an audio CD's disc ID through
[libdiscid](#port-libdiscid), then writes the tags and moves the files into a naming scheme.
Everything except tag editing and renaming needs the MusicBrainz server. It is a PyQt6 program.

## Flags
- `PICARD_DISABLE_AUTOUPDATE=1` — read by the build step of Picard's `setup.py`, which writes
  `autoupdate=False` into the `picard` launcher it generates from `scripts/picard.in`: Picard never
  checks for a new release, and its update-check setting is hidden. The program is updated as a
  package.
- `--no-index` — with `--find-links=vendor`: pip installs only from the unpacked vendor bundle and
  never reaches the network.
- `--no-deps` — pip installs exactly the names on the line, `mutagen discid pyjwt` and Picard
  itself, and resolves nothing more; every other runtime module is a port in `depends`.
- `--no-build-isolation` — build each source distribution with the build backend already
  installed from [python3-setuptools](#port-python3-setuptools) and
  [python3-wheel](#port-python3-wheel), instead of downloading one into a fresh environment.
- `--ignore-installed` — write the named packages into `$PKG` without looking for them in the
  build root. Without it pip finds the copy a previous build of this port left installed and
  uninstalls it from the build root before writing the new one; see
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

## Notes
The bundle, `picard-vendor-<version>.tar.xz`, holds source distributions only: the recipe's
`pypackages = mutagen discid pyjwt`, which `ports/fetch` downloads without resolving their
dependencies; setuptools, the build backend; and Markdown, which `build.sh` does not install. See
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
mutagen, discid and PyJWT are the part of Picard's runtime that no other port imports: mutagen reads and writes
the tags, discid is the ctypes binding to libdiscid behind "Lookup CD", and PyJWT signs the tokens of
the "add release" path, which hands a cluster of files to the MusicBrainz release editor in the
browser. PyQt6, PyYAML, charset-normalizer, tomlkit and Markdown come
from their `python3-*` ports. `pyrequirements = no` stops the fetch reading upstream's pinned
`requirements.txt`, which names those ports' modules, pygit2 and the Windows and macOS packages as
well; `pyruntime = no` has no effect beside `pypackages`.

pygit2 is not a port. Picard imports it only when it is installed, and without it the plugin
manager cannot install plugins from git repositories; the rest of the program runs.

`build.sh` removes Picard's compiled translation catalogues, `picard/locale`, from the package;
its strings are English in the source.

The desktop entry is KDOS's own, written over upstream's. Picard sets its desktop file name, and
so its Wayland app id, to `org.musicbrainz.Picard`, which `StartupWMClass` repeats. It claims no
MIME type: [strawberry](#port-strawberry) is the program audio files open in, and Picard opens
folders from inside.

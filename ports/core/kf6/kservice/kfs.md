## About

KService finds desktop entries and plugins for KDE programs: it reads the `.desktop` files under
`/usr/share/applications` into a binary cache, the system configuration cache (sycoca), and answers
which application opens a MIME type. `kbuildsycoca6` builds that cache. [kio](#port-kio) and
[kparts](#port-kparts) are built on it.

## Notes

[kdoctools](#port-kdoctools) is in `depends` for the manual page of `kbuildsycoca6`, which the build
generates from DocBook when it finds KDocTools; upstream treats it as optional.

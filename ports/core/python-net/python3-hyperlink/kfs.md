## About

Hyperlink is an immutable URL type for Python: it parses, builds, joins and normalises URLs and
converts between the IRI form people read and the URI form sent on the wire.
[python3-twisted](#port-python3-twisted)'s `twisted.python.url` is Hyperlink's `URL` type.

## Notes

Internationalised host names go through [python3-idna](#port-python3-idna), which is in
`depends` for run time.

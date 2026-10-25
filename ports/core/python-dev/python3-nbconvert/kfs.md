## About
nbconvert turns Jupyter notebooks into HTML, Markdown, LaTeX, PDF, reStructuredText and plain
scripts, and can execute a notebook first. This port also carries the notebook stack under it
that [jupyterlab](#port-jupyterlab) and [spyder](#port-spyder) share: `nbformat`, the notebook
file format, with its `fastjsonschema` validator, `nbclient`, which runs a notebook's cells, and
`mistune`, `bleach` and `pandocfilters` for the Markdown and HTML it writes.

## Flags
- `nbclient` — The six PyPI packages named before `.` on the `pip3 install` line (`nbclient`,
  `nbformat`, `fastjsonschema`, `mistune`, `bleach`, `pandocfilters`) are the recipe's pinned
  `pypackages`, installed from its
  [Python vendor bundle](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)
  into this package. Modules other programs import as well, such as `jsonschema`, Beautiful Soup
  and `defusedxml`, are `python3-*` ports in `depends` instead, because two packages cannot own
  one path in `site-packages`.

## Notes
nbconvert's build hook downloads the JupyterLab and classic notebook CSS into its templates, and
fails the build with "Need CSS to proceed." when it cannot. The release tarball already carries
the files, and the hook skips any file it finds, so the offline build needs no network. The
`test -e` on `templates/lab/static/index.css` after the install stops the build if the CSS ever
fails to reach the package.

The PDF exporter runs `xelatex` from [texlive](#port-texlive). The web-PDF exporter needs
Playwright, which has no port, so `--to webpdf` does not work.

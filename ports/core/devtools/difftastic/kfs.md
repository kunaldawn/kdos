## About
[difftastic]{.idx} (`difft`) is a structural diff: it parses both versions of a file with a
tree-sitter grammar for its language and compares the syntax trees, so a reindented block or
a moved line break is not reported as a change. It can stand in for `git diff`'s output through
`GIT_EXTERNAL_DIFF` or `diff.external`.

Every grammar is compiled into the binary at build time, from the `tree-sitter-*` crates in the
vendor bundle and the `vendored_parsers` directory of upstream's source, so syntax-aware diffs
work with no grammar downloaded.

## Notes
The manual page, `difft.1`, is shipped rendered at the top of the source and installed as it is.

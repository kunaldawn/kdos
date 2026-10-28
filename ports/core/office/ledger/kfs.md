## About
[Ledger]{.idx} is double-entry accounting on a plain-text journal. Transactions are lines typed
into a text file, which can live under version control, and `ledger` only reads that file and
prints balances, registers and reports from it; nothing is stored anywhere else. Given a journal
(`-f`, `LEDGER_FILE` or `~/.ledger`) and no command, `ledger` opens an interactive prompt.

## Flags
- `-I$SRC/lib/utfcpp/v4/source` — added to `CXXFLAGS`: the copy of utfcpp, the UTF-8 header
  library, that ledger carries under `lib/utfcpp`. With it on every compile, the unit tests under
  `test/unit` build; without it they fail on `'utf8' has not been declared` after the library
  itself has compiled.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — the programs and `libledger` are installed without a run path;
  everything is in `/usr/lib`.
- `-DBUILD_DOCS=ON` — build the Info manual, `ledger3.info`, with `makeinfo` from
  [texinfo](#port-texinfo). `-DBUILD_WEB_DOCS=OFF` leaves out its HTML and plain-text versions.
- `-DUSE_PYTHON=OFF` — no Python bridge through Boost.Python. It is upstream's default. Boost is
  still required: the configure asks for its date_time, filesystem, iostreams, regex and unit-test
  libraries.
- `-DUSE_GPGME=ON` — read a journal that gpg has encrypted, through
  [gpgmepp](#port-gpgmepp). Off by upstream's default.
- `-DBUILD_LIBRARY=ON` — build and install `libledger` as a shared library beside the program. It
  is upstream's default.
- `-DBOOST_REGEX_UNICODE_RUNS=1` — the result of a configure probe, set instead of probed. The
  probe decides whether ledger matches through Boost.Regex's ICU side, and it assigns a narrow
  string literal to a `std::basic_string<uint32_t>`, which the tree's GCC refuses to compile; it
  would answer no, and case-insensitive matching of non-ASCII account and payee names would fall
  back to bytes. [boost](#port-boost) is built with ICU, so with the result set, `bal активы`
  matches `Активы:Банк`, and an ICU that went missing fails the link instead.

## Notes
The line editor is probed, not chosen by an option: the configure takes libedit when it finds it
and readline otherwise, so the line editing of the interactive prompt would depend on what the build root
happened to hold. `build.sh` checks the generated `system.hh` for `HAVE_EDIT` and stops when
[libedit](#port-libedit) was not found.

`ninja doc` runs before the install: the documentation target is not part of the default build,
and the install rule names the `.info` file whether or not it was built.

After the install, `build.sh` moves upstream's `contrib` directory from `/usr/share/contrib` to
`/usr/share/ledger/contrib`, takes its bash completion out to
`/usr/share/bash-completion/completions/ledger`, and drops the test data of the non-profit audit
reports. The desktop entry is KDOS's own, with `Terminal=true` and `NoDisplay=true`: ledger with
no journal exits at once, so Start does not list it, and it runs from a prompt with `-f`.

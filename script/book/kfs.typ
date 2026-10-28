// ██╗  ██╗██████╗  ██████╗ ███████╗
// ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
// █████╔╝ ██║  ██║██║   ██║███████╗
// ██╔═██╗ ██║  ██║██║   ██║╚════██║
// ██║  ██╗██████╔╝╚██████╔╝███████║
// ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
// ---------------------------------
//   kfs.typ — the PDF of KDOS from Scratch, as a pandoc typst template
//
// pandoc fills the $$…$$ variables from book.md's metadata and puts the body
// where $$body$$ is; typst then compiles the result with no network and only
// its own embedded fonts (kfsrender.py passes --ignore-system-fonts), so the
// same tree gives the same PDF on every machine.
//
// The definitions before the document are what pandoc's typst writer and the
// two filters emit: #divider() for a rule, #kfs-card and #kfs-note from
// kfs.lua, #kfs-idx from index.lua. A name the writer emits and this file
// does not define stops the compile.
//
// Headings carry their numbers in their text, written by kfsgen.py, so typst
// numbers nothing. Level 1 is a part, level 2 a chapter, level 3 a package;
// the contents list goes to level 2, the PDF outline to level 3.
// ---------------------------------

#let kfs-accent = rgb("#146b0a")
#let kfs-dim = rgb("#3d6b44")
#let kfs-rule = rgb("#c9dcc6")
#let kfs-tint = rgb("#f2f8f0")

#let divider() = line(length: 100%, stroke: 0.5pt + kfs-rule)
#let horizontalRule = divider()

#let kfs-idx(term) = [#metadata(term)<kfs-idx>]

#let kfs-card(body) = block(
  width: 100%,
  inset: (left: 9pt, right: 6pt, top: 4pt, bottom: 7pt),
  stroke: (left: 1.5pt + kfs-accent, top: 0.4pt + kfs-rule),
  above: 1.4em,
  below: 0.6em,
  breakable: true,
  {
    set text(size: 9pt)
    set par(justify: false)
    show heading.where(level: 3): it => block(above: 0.3em, below: 0.6em,
      text(font: "DejaVu Sans Mono", size: 10.5pt, weight: "bold", fill: kfs-accent, it.body))
    body
  },
)

#let kfs-note(body) = block(
  width: 100%,
  inset: 7pt,
  radius: 2pt,
  fill: kfs-tint,
  stroke: 0.4pt + kfs-rule,
  breakable: true,
  { set text(size: 0.95em); set par(justify: false); body },
)

// A paragraph of names (a port's installed programs and libraries) is mostly
// code spans with few break points; justified, its lines open wide gaps.
#let kfs-list(body) = { set par(justify: false); body }

$if(highlighting-definitions)$
$highlighting-definitions$
$endif$

#set document(title: "$title$")
#set text(font: "Libertinus Serif", size: 10.5pt, lang: "en", hyphenate: true)
#set par(justify: true, leading: 0.6em, spacing: 0.9em)
#set table(inset: (x: 5pt, y: 3pt), stroke: (x, y) => if y > 0 { (top: 0.3pt + kfs-rule) })
#show table.cell: set text(size: 9pt)
#show table.cell: set par(justify: false)
#show figure.where(kind: table): set block(breakable: true)
// A figure centres its body, and a pandoc table cell with no alignment of its
// own inherits that; a table reads left to right like the text around it.
#show figure.where(kind: table): set align(left)
#show figure: set block(above: 0.8em, below: 0.8em)
#show raw: set text(font: "DejaVu Sans Mono", size: 7.6pt)
// A kernel option's name may break after each underscore, or a long one runs
// out of its table column into the next.
#show raw.where(block: false): it => if it.text.starts-with("CONFIG_") and it.text.contains("_") {
  text(font: "DejaVu Sans Mono", size: 7.6pt, hyphenate: false, it.text.replace("_", "_\u{200B}"))
} else { it }
#show raw.where(block: true): it => block(width: 100%, inset: 6pt, radius: 2pt,
  fill: luma(247), stroke: 0.3pt + luma(225), breakable: true, it)
// An external link and a cross-reference each get a colour, or neither can be
// told from body text on paper.
#show link: it => if type(it.dest) == str { text(fill: kfs-dim, it) } else { text(fill: kfs-accent, it) }
#show heading: set text(font: "DejaVu Sans Mono", fill: kfs-accent, hyphenate: false)
#show heading: set par(justify: false)
#show heading.where(level: 1): it => {
  pagebreak(weak: true)
  v(18%)
  block(below: 2em, text(size: 22pt, weight: "bold", it.body))
}
#show heading.where(level: 2): it => {
  pagebreak(weak: true)
  block(above: 0em, below: 1.2em, text(size: 15pt, weight: "bold", it.body))
}
#show heading.where(level: 3): it => block(above: 1.4em, below: 0.8em,
  text(size: 12pt, weight: "bold", it.body))
#show heading.where(level: 4): it => block(above: 1.2em, below: 0.6em,
  text(size: 10.5pt, weight: "bold", it.body))
#show heading.where(level: 5): it => block(above: 1em, below: 0.5em,
  text(size: 10pt, weight: "bold", it.body))

// ── title page ───────────────────────────────────────────────────────────
#page(margin: (x: 2.4cm, y: 3cm), numbering: none, header: none, footer: none)[
  #v(22%)
  #text(font: "DejaVu Sans Mono", size: 30pt, weight: "bold", fill: kfs-accent)[$title$]
  #v(0.6em)
  #text(size: 14pt)[$subtitle$]
  #v(1fr)
  #line(length: 100%, stroke: 0.6pt + kfs-rule)
  #text(font: "DejaVu Sans Mono", size: 9.5pt)[
    KDOS $kfs-version$ · $kfs-edition$ \
    commit $kfs-commit$ · $kfs-date$ \
    build-state snapshot $kfs-snapshot$
  ]
]

// ── running heads ────────────────────────────────────────────────────────
// The header is laid out at the top of its page, so "before here" misses a
// heading further down the same page: a package that starts on this page is
// found by looking after here, and named in preference to the one before.
#let kfs-before(level) = {
  let hs = query(selector(heading.where(level: level)).before(here()))
  if hs.len() > 0 { hs.last() } else { none }
}
#let kfs-on-page(level) = {
  let hs = query(selector(heading.where(level: level)).after(here()))
  if hs.len() > 0 and hs.first().location().page() == here().page() { hs.first() } else { none }
}
#let kfs-after(a, b) = a != none and (b == none or a.location().page() > b.location().page()
  or (a.location().page() == b.location().page()
      and a.location().position().y > b.location().position().y))

#set page(
  paper: "a4",
  margin: (inside: 2.3cm, outside: 1.9cm, top: 2.2cm, bottom: 2cm),
  numbering: "1",
  header: context {
    if kfs-on-page(1) != none or kfs-on-page(2) != none { return }
    let pt = kfs-before(1)
    let ch = kfs-before(2)
    let pk = kfs-on-page(3)
    if pk == none { pk = kfs-before(3) }
    set text(size: 8pt, fill: kfs-dim, font: "DejaVu Sans Mono")
    let inpart = kfs-after(ch, pt)
    let chap = if inpart { ch.body } else if pt != none { pt.body } else { [] }
    let port = if inpart and kfs-after(pk, ch) { pk.body } else { [] }
    if calc.even(here().page()) { grid(columns: (1fr, auto), chap, port) } else { grid(columns: (1fr, auto), port, chap) }
    line(length: 100%, stroke: 0.3pt + kfs-rule)
  },
  footer: context {
    set text(size: 8.5pt, fill: kfs-dim)
    align(center, counter(page).display())
  },
)
#counter(page).update(1)

#outline(title: [Contents], depth: 2, indent: auto)

$body$

// ── index ────────────────────────────────────────────────────────────────
#heading(level: 1, outlined: true)[Index]
// Terms that differ only in case are one entry, printed in the spelling most
// spans use (the first in sort order on a tie): a port page's generated span
// says `pipewire` where the prose says `PipeWire`, and two entries would split
// the port page from the pages that explain it. kfsrender.py folds the HTML
// index by the same rule; the two indexes list the same entries.
#context {
  let groups = (:)
  for m in query(<kfs-idx>) {
    let t = m.value
    let fold = lower(t)
    let key = upper(t.first())
    if key.match(regex("[A-Z]")) == none { key = "#" }
    let entry = groups.at(key, default: (:))
    let e = entry.at(fold, default: (pages: (), names: (:)))
    let pages = e.pages
    let names = e.names
    names.insert(t, names.at(t, default: 0) + 1)
    let pg = counter(page).at(m.location()).first()
    if pages.find(x => x.at(0) == pg) == none { pages.push((pg, m.location())) }
    entry.insert(fold, (pages: pages, names: names))
    groups.insert(key, entry)
  }
  set text(size: 8pt)
  set par(justify: false, spacing: 0.35em)
  columns(3, gutter: 1.2em, {
    for key in groups.keys().sorted() {
      block(above: 1em, below: 0.4em, text(font: "DejaVu Sans Mono", weight: "bold", fill: kfs-accent, key))
      let entry = groups.at(key)
      for fold in entry.keys().sorted() {
        let e = entry.at(fold)
        let t = e.names.pairs().sorted(key: p => p.at(0)).sorted(key: p => -p.at(1)).first().at(0)
        let refs = e.pages.map(((pg, loc)) => link(loc, str(pg))).join(", ")
        par(hanging-indent: 1em)[#t #h(0.4em) #refs]
      }
    }
  })
}

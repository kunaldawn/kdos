#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   bookcounts — the book's port counts, recomputed from the tree
# ---------------------------------
#
# Run from the repository root; testing/docscheck.sh runs it. Three pages carry
# exact counts of the recipes, shelves and package lists:
# docs/kdos/06-reference/status.md, docs/kdos/06-reference/ports-catalogue.md
# and docs/kdos/03-architecture/packaging.md. Every other page says "about
# 2,000" or links to one of them, and this check recomputes each such figure
# the three state:
#
#   - recipes: directories holding a kpkgbuild, in ports/core and per src/ area
#   - shelves: the non-comment lines of ports/shelves
#   - the package lists: lines, distinct names, ports named by more than one
#     phase, and per phase the names and the ports it installs, resolved over
#     `depends =` the way testing/phaseclosure.py resolves them
#   - the catalogue: every recipe exactly once, under its shelf or src/ area,
#     with the recipe's version and the phase that installs it; each shelf's
#     count in its heading and in the per-shelf table; the ports named in more
#     than one list; and that every port it lists as the application-developer
#     platform is still named by no `depends =`
#
# The vendoring counts (recipes that set `vendoring =`, per ecosystem, and
# ports carrying a vendor bundle) are stated exactly on principles.md,
# glossary.md, writing-ports.md, how-kdos-is-built.md and packaging.md, and
# nothing here recomputes them: a recipe that gains or drops `vendoring =`
# leaves those figures wrong until a page is edited by hand.
#
# A recipe that no phase installs, from a list or by a 00_cross/10_bootstrap
# step, is refused unless ports/unbuilt names it, and a name there that a
# phase does install, or that has no recipe, is refused too: the catalogue
# states that every recipe is built but those, and marks each `not built`.
#
# A figure is found by the sentence or table row that states it. When a page's
# wording changes so that a pattern below no longer matches, the check fails
# with "not found" rather than passing silently; change the pattern with the
# page. Exit status 1 on any failure.

import importlib.util
import os
import re
import sys

ROOT = os.getcwd()
CATALOGUE = 'docs/kdos/06-reference/ports-catalogue.md'
STATUS = 'docs/kdos/06-reference/status.md'
PACKAGING = 'docs/kdos/03-architecture/packaging.md'
UNBUILT = 'ports/unbuilt'
SRC_AREAS = ['system', 'art', 'desktop', 'daemons']
PLATFORM_HEADING = 'The application-developer platform'

fails = []


def fail(msg):
    fails.append(msg)


def num(s):
    return int(s.replace(',', ''))


def load_phaseclosure():
    path = os.path.join(ROOT, 'testing', 'phaseclosure.py')
    spec = importlib.util.spec_from_file_location('phaseclosure', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def read_key(path, key):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                m = re.match(r'^' + key + r'\s*=\s*(.*?)\s*$', line)
                if m:
                    return m.group(1)
    except OSError:
        pass
    return None


def recipes():
    """name -> (home, version, depends); home is a shelf or src/<area>."""
    out = {}
    core = os.path.join(ROOT, 'ports', 'core')
    for shelf in sorted(os.listdir(core)):
        sd = os.path.join(core, shelf)
        if shelf.startswith('.') or not os.path.isdir(sd):
            continue
        for name in sorted(os.listdir(sd)):
            kb = os.path.join(sd, name, 'kpkgbuild')
            if os.path.isfile(kb):
                out[name] = (shelf, kb)
    for area in SRC_AREAS:
        ad = os.path.join(ROOT, 'src', area)
        if not os.path.isdir(ad):
            continue
        for name in sorted(os.listdir(ad)):
            kb = os.path.join(ad, name, 'kpkgbuild')
            if os.path.isfile(kb):
                out[name] = ('src/' + area, kb)
    return {n: (home, read_key(kb, 'version'), (read_key(kb, 'depends') or '').split())
            for n, (home, kb) in out.items()}


def phases(pc):
    """Resolve every package phase as phaseclosure does: (dir, lines, names, installs, files)."""
    base = os.path.join(ROOT, 'script', 'phases')
    res = pc.Resolver()
    installed = {}
    out = []
    for d in sorted(os.listdir(base)):
        pd = os.path.join(base, d)
        txt = os.path.join(pd, 'packages.txt')
        dd = os.path.join(pd, 'packages.d')
        if os.path.isfile(txt):
            files, split = [txt], False
        elif os.path.isdir(dd):
            files = [os.path.join(dd, f) for f in sorted(os.listdir(dd))
                     if f.endswith('.txt') and not f.startswith('.')]
            split = True
        else:
            continue
        _, repos = pc.read_repos(ROOT, pd)
        lines = [n for n, _, _, _ in pc.read_list(files, split)]
        emitted = []
        seen = set()

        def walk(n):
            pdir = res.find(repos, n)
            if not pdir:
                return
            for dep in pc.read_depends(pdir):
                if dep in seen or dep in installed:
                    continue
                seen.add(dep)
                walk(dep)
                if dep not in emitted:
                    emitted.append(dep)

        for n in lines:
            if n in seen or n in installed:
                continue
            seen.add(n)
            walk(n)
            if n not in emitted:
                emitted.append(n)
        for p in emitted:
            installed[p] = d
        out.append((d, lines, set(lines), emitted, len(files) if split else 0))
    return out


def by_script(rec):
    """Recipes a 00_cross or 10_bootstrap step compiles from their port: name -> phase."""
    out = {}
    base = os.path.join(ROOT, 'script', 'phases')
    for d in sorted(os.listdir(base)):
        if not re.match(r'^(00|10)_', d):
            continue
        for f in sorted(os.listdir(os.path.join(base, d))):
            if not f.endswith('.sh'):
                continue
            with open(os.path.join(base, d, f), encoding='utf-8', errors='replace') as fh:
                for m in re.finditer(r'SRC=\$WORKSPACE/src/[a-z]+/([A-Za-z0-9._+-]+)', fh.read()):
                    if m.group(1) in rec:
                        out.setdefault(m.group(1), d)
    return out


def unbuilt():
    """The names ports/unbuilt lists, in file order."""
    out = []
    try:
        with open(os.path.join(ROOT, UNBUILT), encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    out.append(line)
    except OSError:
        pass
    return out


def flat(path):
    with open(os.path.join(ROOT, path), encoding='utf-8') as f:
        return f.read()


def expect(page, text, pattern, values, label):
    """Match `pattern` in whitespace-collapsed `text`; compare its groups to `values`."""
    m = re.search(pattern, re.sub(r'\s+', ' ', text))
    if not m:
        fail(f'{page}: not found: {label}')
        return
    got = [num(g) for g in m.groups()]
    if got != list(values):
        want = ', '.join(f'{v:,}' for v in values)
        have = ', '.join(f'{v:,}' for v in got)
        fail(f'{page}: {label}: the page says {have}, the tree gives {want}')


def phase_table(page, text, ph):
    """Every `| \\`NN_x\\` | ... | names | installs |` row against the resolution, and a
    `packages.d/`, N files` cell against the files in that directory."""
    rows = {}
    nfiles = {}
    for line in text.split('\n'):
        m = re.match(r'^\| `(\d\d_[a-z]+)` \|(.*)\|\s*$', line)
        if not m:
            continue
        cells = [c.strip() for c in m.group(2).split('|')]
        if len(cells) >= 2 and re.match(r'^[\d,]+$', cells[-1]) and re.match(r'^[\d,]+$', cells[-2]):
            rows[m.group(1)] = (num(cells[-2]), num(cells[-1]))
            for c in cells:
                fm = re.match(r'^`packages\.d/`, (\d+) files$', c)
                if fm:
                    nfiles[m.group(1)] = int(fm.group(1))
    for d, lines, _, emitted, files in ph:
        if d in nfiles and nfiles[d] != files:
            fail(f'{page}: phase table {d}: the page says {nfiles[d]} files in packages.d/; '
                 f'the tree has {files}')
        if d not in rows:
            fail(f'{page}: the phase table has no row for {d}')
        elif rows[d] != (len(lines), len(emitted)):
            fail(f'{page}: phase table {d}: the page says {rows[d][0]} names, {rows[d][1]} installs; '
                 f'the tree gives {len(lines)}, {len(emitted)}')


def sections(text):
    """Heading title -> body text up to the next heading of any level."""
    out = {}
    cur = None
    for line in text.split('\n'):
        m = re.match(r'^#{2,3} (.*?)\s*$', line)
        if m:
            cur = m.group(1)
            out[cur] = []
        elif cur is not None:
            out[cur].append(line)
    return {k: '\n'.join(v) for k, v in out.items()}


def main():
    pc = load_phaseclosure()
    rec = recipes()
    ph = phases(pc)
    script = by_script(rec)

    first = {}
    for d, _, _, emitted, _ in ph:
        for p in emitted:
            first.setdefault(p, d)
    named_by = {}
    for d, _, names, _, _ in ph:
        for n in names:
            named_by.setdefault(n, []).append(d)

    core = sorted(n for n, r in rec.items() if not r[0].startswith('src/'))
    per_area = {a: sum(1 for r in rec.values() if r[0] == 'src/' + a) for a in SRC_AREAS}
    n_src = sum(per_area.values())
    n_rec = len(rec)
    with open(os.path.join(ROOT, 'ports', 'shelves'), encoding='utf-8') as f:
        n_shelves = sum(1 for l in f if l.strip() and not l.startswith('#'))
    lines = sum(len(l) for _, l, _, _, _ in ph)
    distinct = len(named_by)
    multi = sorted(n for n, ds in named_by.items() if len(ds) > 1)
    reached = sum(len(e) for _, _, _, e, _ in ph)
    core_installed = sum(1 for n in core if n in first or n in script)
    selfhost = next((x for x in ph if x[0] == '20_selfhost'), None)

    skip = unbuilt()
    for n in sorted(rec):
        if n not in first and n not in script and n not in skip:
            fail(f'{n}: no phase installs this recipe; name it in the list of the phase its '
                 f'dependencies put it in, or in {UNBUILT}')
    for n in skip:
        if n not in rec:
            fail(f'{UNBUILT}: {n} has no recipe')
        elif n in first or n in script:
            fail(f'{UNBUILT}: {n} is installed by {first.get(n) or script.get(n)}; '
                 f'remove it from the list')

    # ports-catalogue.md
    page = CATALOGUE
    text = flat(page)
    expect(page, text, r'each of the ([\d,]+) recipes under `ports/core` and the ([\d,]+) recipes '
           r'KDOS writes itself under `src/`, ([\d,]+) in all', [len(core), n_src, n_rec],
           'the opening recipe counts')
    expect(page, text, r'The ([\d,]+) shelves form a closed list', [n_shelves], 'the shelf count')
    phase_table(page, text, ph)
    expect(page, text, r'\| Total \| \| \| ([\d,]+) \| ([\d,]+) \|', [lines, reached],
           'the phase table total')
    expect(page, text, r'that is every recipe but ([\d,]+), listed in `ports/unbuilt`', [len(skip)],
           'the recipes not built')
    expect(page, text, r'The lists hold ([\d,]+) lines naming ([\d,]+) distinct ports; ([\d,]+) of '
           r'those ports are named by more than one phase\. Following `depends =` from the lists '
           r'reaches ([\d,]+) ports', [lines, distinct, len(multi), reached], 'the list totals')
    if selfhost:
        pulled = [p for p in selfhost[3] if p not in selfhost[2]]
        unlisted = [p for p in pulled if p not in named_by]
        expect(page, text, r'its closure brings in ([\d,]+) ports its list does not name\. Of those, '
               r'([\d,]+) are named by no list at all', [len(pulled), len(unlisted)],
               "20_selfhost's dependency closure")

    sec = sections(text)
    shelf_table = {}
    for line in sec.get('Ports per shelf', '').split('\n'):
        m = re.match(r'^\| \[`([^`]+)`\]\([^)]*\) \| ([\d,]+) \|', line)
        if m:
            shelf_table[m.group(1)] = num(m.group(2))
    homes = sorted(set(r[0] for r in rec.values()))
    for h in homes:
        count = sum(1 for r in rec.values() if r[0] == h)
        if shelf_table.get(h) != count:
            fail(f'{page}: Ports per shelf: {h} says {shelf_table.get(h)}, the tree gives {count}')
        body = sec.get(h)
        if body is None:
            fail(f'{page}: no section for {h}')
            continue
        m = re.search(r'([\d,]+) ports?\b', re.sub(r'\s+', ' ', body.split('\n|')[0]))
        if not m or num(m.group(1)) != count:
            fail(f'{page}: the {h} heading paragraph does not say {count} ports')
    for h in shelf_table:
        if h not in homes:
            fail(f'{page}: Ports per shelf lists {h}, which holds no recipe')
    expect(page, text, r'\| Total \| ([\d,]+) \|', [n_rec], 'the per-shelf total')

    seen = {}
    for title, body in sec.items():
        if title not in homes:
            continue
        for line in body.split('\n'):
            m = re.match(r'^\| `([^`]+)` \| ([^|]*) \| [^|]* \| ([^|]*) \|\s*$', line)
            if not m:
                continue
            seen.setdefault(m.group(1), []).append((title, m.group(2).strip(), m.group(3).strip()))
    for n, (home, ver, _) in sorted(rec.items()):
        got = seen.get(n, [])
        if len(got) != 1:
            fail(f'{page}: {n} has {len(got)} catalogue rows; it needs exactly one, under {home}')
            continue
        title, cver, cphase = got[0]
        if title != home:
            fail(f'{page}: {n} is catalogued under {title}; its recipe is in {home}')
        if cver != ver:
            fail(f'{page}: {n} is catalogued at {cver}; its recipe says {ver}')
        if n in script:
            want = f'`{script[n]}` (by script)'
        elif n in first:
            want = f'`{first[n]}`'
            if first[n] not in named_by.get(n, []):
                want += ' (as a dependency)'
            again = [d for d in named_by.get(n, []) if d != first[n]]
            if again:
                want += ', named again by ' + ', '.join(f'`{d}`' for d in again)
        else:
            want = 'not built'
        if cphase != want:
            fail(f'{page}: {n}: the phase column says {cphase}; the tree gives {want}')
    for n in sorted(seen):
        if n not in rec:
            fail(f'{page}: {n} is catalogued and has no recipe')

    listed_multi = sorted(re.findall(r'^\| `([^`]+)` \|', sec.get('Ports named in more than one list', ''),
                                     re.M))
    if listed_multi != multi:
        fail(f'{page}: Ports named in more than one list gives {", ".join(listed_multi)}; '
             f'the tree gives {", ".join(multi)}')

    consumers = {}
    for n, r in rec.items():
        for dep in r[2]:
            consumers.setdefault(dep, []).append(n)
    platform = re.findall(r'^\| `([^`]+)` \|', sec.get(PLATFORM_HEADING, ''), re.M)
    if not platform:
        fail(f'{page}: no "{PLATFORM_HEADING}" table')
    for n in platform:
        if n not in rec:
            fail(f'{page}: platform port {n} has no recipe')
        elif consumers.get(n):
            fail(f'{page}: platform port {n} is named by `depends =` of '
                 f'{", ".join(sorted(consumers[n]))}; move it out of the platform table')

    # status.md
    page = STATUS
    text = flat(page)
    expect(page, text, r'\| Port recipes \| ([\d,]+) \|', [n_rec], 'Scale: port recipes')
    expect(page, text, r'\| — in `ports/core` \| ([\d,]+) \|', [len(core)], 'Scale: ports/core')
    for a in SRC_AREAS:
        expect(page, text, r'\| — in `src/' + a + r'` \| ([\d,]+) \|', [per_area[a]], f'Scale: src/{a}')
    expect(page, text, r'\| Shelves \| ([\d,]+) \|', [n_shelves], 'Scale: shelves')
    expect(page, text, r'\| The ports tree \| [A-Za-z ]+ \| ([\d,]+) recipes, ([\d,]+) of them upstream '
           r'ports on ([\d,]+) shelves', [n_rec, len(core), n_shelves], 'the ports tree row')
    userland = sum(len(l) for d, l, _, _, _ in ph if '40_lang' <= d <= '44_apps')
    expect(page, text, r'`40_lang` to `44_apps`, name ([\d,]+) recipes', [userland],
           'the userland phases')
    expect(page, text, r'Of the ([\d,]+) recipes, [\d,]+ are absent from the build tree', [n_rec],
           'the ports tree paragraph')

    # packaging.md
    page = PACKAGING
    text = flat(page)
    expect(page, text, r'\| `/ports/core` \| `ports/core/` \| ([\d,]+) \| Upstream software, filed on '
           r'([\d,]+) shelves', [len(core), n_shelves], 'repository table: ports/core')
    for a in SRC_AREAS:
        expect(page, text, r'\| `/kdos/src/' + a + r'` \| `src/' + a + r'/` \| ([\d,]+) \|',
               [per_area[a]], f'repository table: src/{a}')
    expect(page, text, r'That is ([\d,]+) recipes in all', [n_rec], 'the recipe total')
    phase_table(page, text, ph)
    expect(page, text, r'the lists name ([\d,]+) distinct ports and install ([\d,]+): ([\d,]+) of the '
           r'([\d,]+) in `ports/core`', [distinct, reached, core_installed, len(core)],
           'the list totals')

    for f in fails:
        print('BOOKCOUNT ' + f)
    if fails:
        return 1
    print(f'bookcounts: ok ({n_rec:,} recipes, {lines:,} list lines naming {distinct:,} ports)')
    return 0


if __name__ == '__main__':
    sys.exit(main())

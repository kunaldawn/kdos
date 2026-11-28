#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   mksite — kdos.dev's homepage and the repository's README.md, from one source
#
#     mksite.py [--out DIR]             write README.md, and the homepage into DIR
#                                       (build/site when no DIR is named)
#     mksite.py --check [--out DIR]     write no README.md, and exit 1 with a diff
#                                       when it is not what this writes; the
#                                       homepage only when DIR is named
#
#   Run from anywhere; every path is the checkout's.
# ---------------------------------
#
# ONE SOURCE, TWO OUTPUTS. site/home.md is the page's text. It becomes
# README.md, GitHub-flavoured Markdown that github.com renders with no script
# and no stylesheet, and DIR/index.html, site/template.html filled with the
# same text as HTML and dressed by site/style.css and site/site.js. A README
# edited by hand is a second copy that goes stale, so --check fails on any
# difference, and `make check` and the pre-push hook run it.
#
# THE NUMBERS ARE READ, NEVER WRITTEN. `{{NAME}}` in home.md (and in the
# template) is a fact this script takes from the tree: the release from
# fs/etc/os-release, the port count (ports/core and KDOS's own recipes under
# src/) and the catalogue's, the Plasma, Frameworks, Gear, kernel and musl
# versions from their recipes, the accents from libkcolor's KCOL_SCHEMES, the
# licences from KDOS's own recipes and the bootstrap step that compiles kpkg,
# the developer from LICENSE, the disk sizes from kinstall-core and
# ports/RELEASE, and the release key's id, the book's parts and a build's
# disk from the book. A sum or a size
# is joined to its unit by a no-break space, so a narrow screen never strands
# the unit on a line of its own. A name it does not know stops the run, and so does
# a fact it cannot find: a page that states a number nothing checks is the
# page that ends up wrong.
#
# THE MARKDOWN IS A SMALL SUBSET, the part both outputs can carry:
#
#   ---  key: value lines  ---      front matter, first thing in the file:
#                                   title, description, nav
#   <!-- … -->                      a comment; in neither output
#   ## Heading / ### Heading        a section of the page / a heading in it;
#                                   both outputs give it GitHub's id
#   paragraphs, - and 1. lists      one level; an indented line continues
#   | tables |                      GitHub's pipe tables
#   ```lang … ```                   a code block
#   `code` **bold** *em*            inline
#   [text](target), ![alt](target)  a target is #id, http(s):, mailto:, or
#                                   book:PAGE.md#id  a page of docs/kdos
#                                   kfs:             KDOS from Scratch, and
#                                   kfs:kfs.pdf      its PDF, both on the site
#                                                    from either output: the
#                                                    package pages exist only
#                                                    in the rendered book
#                                   repo:PATH        a file of the checkout
#                                   asset:PATH       a file of site/assets
#                                   and each is checked: the file exists and
#                                   the #id is a heading of that page
#
# and a few block directives, `::: name` to a line of `:::`:
#
#   hero       subtitle:, pitch:, button: Label | target (repeatable), note:
#   badges     names of the 88x31 buttons, badge-<name>.png in site/assets/art
#   facts      a two-column table with an empty header row, set as a
#              specification sheet; the README sets it as an HTML table, since
#              GitHub draws an empty Markdown header as a blank row
#   terminal   KDOS commands whose output is drawn from the facts:
#              `kdos version`, `kdos theme list`
#   gallery    `group: Title` lines, each followed by shot names from
#              site/assets/shots/shots.tsv; every shot is shown somewhere.
#              `colour: NAME…` names shots whose colours are what they show,
#              which Phosphor mode leaves untinted, as it leaves every
#              accent-<name> shot; an animated shot needs <name>-still.webp,
#              its still at the same size
#   accents    the accent picker; `default-shot: NAME` is the picture for the
#              default accent, which has no accent-<name> shot of its own,
#              `default-caption:` the words under it, since that shot's own
#              caption is about something else, and `crt-shot: NAME` the
#              picture Phosphor mode shows. The README sets it as a table
#              of the accents' cards, each opening its accent's picture,
#              and leaves the accent-<name> shots out of its gallery
#   cards      `### Title`, `icon: NAME` (pixel-NAME.png), then text
#   steps      `### Title`, then text: numbered steps
#   note       a call-out
#   html, readme   Markdown that goes into one output only
#   js         Markdown for the homepage that describes what site.js does,
#              shown only once it runs: without it the sentence is untrue
#
# The pictures' manifests are read too: site/assets/art/art.tsv for each
# picture's alt text (site/art/genart.py writes it), and
# site/assets/shots/shots.tsv for each screenshot's title, caption and alt
# text (testing/siteshots.sh writes it).
#
# Either mode also refuses a page that would mislead or leak: an accent whose
# text would fall under WCAG AA contrast, artwork drawn from a palette, logo,
# penguin or mark that has changed since (site/assets/art/inputs.tsv), and a
# page, stylesheet or script that would ask another host for anything.
#
# THE DEPLOYED SITE is DIR as .github/workflows/books.yml composes it: the
# KDOS book in DIR/book, KDOS from Scratch in DIR/kfs, then this script.
# When DIR/book holds the book, every page of it also gets a stub at the
# site's root, DIR/<page>.html, that sends the reader on to
# DIR/book/<page>.html with its #fragment, so a link to a book page at the
# root still lands; a stub never replaces a file of the homepage. sitemap.xml
# lists the homepage and both books. When DIR/kfs holds KDOS from Scratch as
# well, every link the book makes into it (mkbook.py --site) must land on a
# page and an id that book has, or the run stops before anything deploys.
#
# Standard library only, so it runs on a bare host and in CI. Nothing is
# fetched, and the page it writes requests nothing from another host.
# ---------------------------------

import argparse
import colorsys
import difflib
import glob
import hashlib
import html
import json
import os
import re
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SITE = os.path.join(ROOT, 'site')
HOME = os.path.join(SITE, 'home.md')
TEMPLATE = os.path.join(SITE, 'template.html')
STYLE = os.path.join(SITE, 'style.css')
SCRIPT = os.path.join(SITE, 'site.js')
ASSETS = os.path.join(SITE, 'assets')
README = os.path.join(ROOT, 'README.md')
BOOK = os.path.join(ROOT, 'docs', 'kdos')

CANONICAL = 'https://kdos.dev/'
REPO = 'https://github.com/kunaldawn/kdos'
BLOB = REPO + '/blob/main/'
TREE = REPO + '/tree/main/'

KCOL_FIELDS = ('primary', 'dim', 'secondary', 'urgent', 'deep', 'text',
               'variant', 'pdark', 'backdrop')
# The CSS custom property each libkcolor field becomes.
CSS_VAR = {'primary': 'pri', 'dim': 'dim', 'secondary': 'sec',
           'urgent': 'urg', 'deep': 'deep', 'text': 'text',
           'variant': 'var', 'pdark': 'pdark', 'backdrop': 'bd'}
WORDS = ('zero one two three four five six seven eight nine ten eleven '
         'twelve').split()


class SiteError(Exception):
    pass


def die(msg):
    raise SiteError(msg)


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def rel(path):
    return os.path.relpath(path, ROOT)


# ── facts ────────────────────────────────────────────────────────────────

def recipe_field(path, key):
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.match(r'%s\s*=\s*(.*?)\s*$' % re.escape(key), line)
            if m:
                return m.group(1)
    die('%s: no %s =' % (rel(path), key))


def recipe(name):
    hits = glob.glob(os.path.join(ROOT, 'ports', 'core', '*', name, 'kpkgbuild'))
    if len(hits) != 1:
        die('port %s: %d recipes, want one' % (name, len(hits)))
    return hits[0]


def version_of(name):
    return recipe_field(recipe(name), 'version')


def lines_of(path):
    """The lines of a list file that are not blank or a comment."""
    return [l.strip() for l in read(path).splitlines()
            if l.strip() and not l.lstrip().startswith('#')]


def os_release():
    out = {}
    for line in read(os.path.join(ROOT, 'fs', 'etc', 'os-release')).splitlines():
        m = re.match(r'([A-Z_]+)=(.*)$', line)
        if m:
            out[m.group(1)] = m.group(2).strip().strip('"')
    for key in ('VERSION_ID', 'PRETTY_NAME'):
        if not out.get(key):
            die('fs/etc/os-release: no %s' % key)
    return out


def read_schemes():
    """libkcolor's KCOL_SCHEMES rows in table order, and the default's name."""
    path = os.path.join(ROOT, 'src', 'libs', 'libkcolor', 'kcolor.h')
    text = read(path)
    m = re.search(r'#define\s+KCOL_SCHEMES\(X\)(.*?)(?:\n\s*\n|\n#)', text, re.S)
    if not m:
        die('%s: no KCOL_SCHEMES table' % rel(path))
    body = m.group(1).replace('\\\n', ' ')
    out = []
    for name, label, theme, vals in re.findall(
            r'X\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*([^)]*)\)', body):
        hexes = [v.strip().lower() for v in vals.split(',')]
        if len(hexes) != len(KCOL_FIELDS) or \
                not all(re.fullmatch(r'[0-9a-f]{6}', h) for h in hexes):
            die('%s: %s is not nine colours' % (rel(path), name))
        out.append({'name': name, 'label': label, 'theme': theme,
                    'title': name.capitalize(),
                    'hex': dict(zip(KCOL_FIELDS, hexes))})
    d = re.search(r'#define\s+KCOL_DEFAULT_ID\s+(\w+)', text)
    if not out or not d or d.group(1) not in [s['name'] for s in out]:
        die('%s: no schemes, or no default among them' % rel(path))
    return out, d.group(1)


# site/art/genart.py draws the hero, the badges, the accent cards and the
# README's pictures once, and they are committed; the page's own colours are
# read on every run. inputs.tsv records what the pictures were drawn from, so
# a palette, wordmark, penguin or mark changed since then stops this script
# instead of leaving the pictures in the old colours beside a page in the
# new ones. The palette is hashed as genart.py's schemes_text() writes it.
ART_SCHEMES = 'src/libs/libkcolor/kcolor.h KCOL_SCHEMES'


def schemes_text(schemes, default):
    return ''.join('%s\t%s\t%s\t%s\n' % (s['name'], s['label'], s['theme'],
                                         ','.join(s['hex'][f] for f in KCOL_FIELDS))
                   for s in schemes) + 'default\t%s\n' % default


def check_art_inputs(schemes, default):
    path = os.path.join(ASSETS, 'art', 'inputs.tsv')
    rows = read_tsv(path)
    if ART_SCHEMES not in [r['input'] for r in rows]:
        die('%s: no row for %s' % (rel(path), ART_SCHEMES))
    for r in rows:
        if r['input'] == ART_SCHEMES:
            data = schemes_text(schemes, default).encode('utf-8')
        else:
            src = os.path.join(ROOT, r['input'])
            if not os.path.isfile(src):
                die('%s: %s does not exist' % (rel(path), r['input']))
            with open(src, 'rb') as f:
                data = f.read()
        if hashlib.sha256(data).hexdigest() != r['sha256']:
            die('site/assets/art was drawn from another %s: run site/art/genart.py'
                % r['input'])


def crt_pairs():
    """The accents `kdos phosphor on` trades for phosphor, as phosphor.c
    names them: the page's Phosphor mode does what the desktop's does."""
    path = os.path.join(ROOT, 'src', 'system', 'kdos-tools', 'phosphor.c')
    m = re.search(r'\bpair\s*=\s*(!?strcmp\([^;]*);', read(path))
    names = re.findall(r'strcmp\(\s*accent\s*,\s*"(\w+)"\s*\)', m.group(1)) if m else []
    if not names:
        die('%s: no `pair =` naming the accents phosphor replaces' % rel(path))
    return names


def luminance(h):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (ch(int(h[i:i + 2], 16)) for i in (0, 2, 4))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


# The colour pairs the stylesheet sets text in, and the WCAG floor each must
# clear in every accent: body text and links on the page's ground, on a card
# and on the alternate sections' ground; a filled button is the page's
# ground on the primary, the second pair again. libkcolor's self-test holds
# the first two; a scheme that fails one here would print an unreadable page.
CONTRAST_FLOORS = (('text', 'deep', 7.0), ('primary', 'deep', 4.5),
                   ('text', 'variant', 4.5), ('primary', 'variant', 4.5),
                   ('text', 'backdrop', 4.5), ('primary', 'backdrop', 4.5))


def check_contrast(schemes):
    for s in schemes:
        for a, b, floor in CONTRAST_FLOORS:
            c = contrast(s['hex'][a], s['hex'][b])
            if c < floor:
                die('accent %s: %s on %s is %.2f:1, under %.1f:1'
                    % (s['name'], a, b, c, floor))


def catalogue_apps():
    path = os.path.join(ROOT, 'src', 'system', 'kdos-appbox', 'catalogue')
    n = sum(1 for l in lines_of(path) if l.split()[0] == 'app')
    if not n:
        die('%s: no app rows' % rel(path))
    return n


def own_licences():
    """{SPDX: [name, …]} for every KDOS port whose licence is not plain MIT."""
    out = {}
    for area in ('system', 'art', 'desktop', 'daemons'):
        for path in glob.glob(os.path.join(ROOT, 'src', area, '*', 'kpkgbuild')):
            lic = recipe_field(path, 'license')
            if lic != 'MIT':
                out.setdefault(lic, []).append(recipe_field(path, 'name'))
    return {k: sorted(v) for k, v in out.items()}


# kpkg is compiled by a bootstrap step, not by a recipe, so own_licences()
# cannot see what it is made of: a step that compiles libksig's vendored
# Monocypher into a program it installs puts that program beside the recipes
# whose code is MIT and another project's.
def bootstrap_mixed():
    out = []
    for path in sorted(glob.glob(os.path.join(ROOT, 'script', 'phases', '*', '*.sh'))):
        text = read(path)
        if 'libksig/monocypher/' not in text:
            continue
        names = re.findall(r'^install\s+-D?m755\s+\S+\s+\S*/usr/bin/([\w.+-]+)\s*$', text, re.M)
        if len(names) != 1:
            die('%s: compiles Monocypher and installs %d programs into /usr/bin, want one'
                % (rel(path), len(names)))
        out.append(names[0])
    return out


def author():
    """The copyright holder LICENSE names."""
    m = re.search(r'^Copyright \(c\) \d{4}(?:-\d{4})? (.+?)\s*$', read(os.path.join(ROOT, 'LICENSE')),
                  re.M)
    if not m:
        die('LICENSE: no "Copyright (c) YEAR NAME" line')
    return m.group(1)


def and_list(items):
    return items[0] if len(items) == 1 else ', '.join(items[:-1]) + ' and ' + items[-1]


NBSP = ' '


def unit(text):
    """'24.5 GiB' with a no-break space between the number and its unit."""
    return re.sub(r'(\d) (k?B|[KMGT]i?B)\b', '\\1' + NBSP + '\\2', text)


def iso_limit():
    """The release gate on the ISO's size, from the Makefile, in GB."""
    m = re.search(r'^KDOS_ISO_MAX\s*\?=\s*(\d+)\s*$', read(os.path.join(ROOT, 'Makefile')), re.M)
    if not m:
        die('Makefile: no KDOS_ISO_MAX')
    tenths = -(-int(m.group(1)) // 100000000)
    return unit(('%d GB' % (tenths // 10)) if tenths % 10 == 0 else ('%.1f GB' % (tenths / 10)))


def build_disk():
    """(the sources, everything a first build needs) in GB, from what the
    Developing chapter says the sources and a complete build take: the
    sum is rounded up to ten, so the page never promises less than the
    book."""
    path = os.path.join(BOOK, '05-developer', 'developing.md')
    text = ' '.join(read(path).split())
    src = re.search(r'The upstream sources for the current tree take about ([\d.]+) GB', text)
    out = re.search(r'A complete build adds about (\d+) GB under `build/`', text)
    if not src or not out:
        die('%s: no "sources … take about N GB" and "A complete build adds about N GB '
            'under `build/`"' % rel(path))
    total = -(-(float(src.group(1)) + int(out.group(1))) // 10) * 10
    return unit('%d GB' % round(float(src.group(1)))), unit('%d GB' % total)


def book_parts():
    n = len(re.findall(r'^### Part [IVXL]+ ', read(os.path.join(BOOK, 'README.md')), re.M))
    if not n:
        die('docs/kdos/README.md: no "### Part" headings under ## Contents')
    return n


def release_key_id():
    path = os.path.join(BOOK, '02-user-guide', 'getting-started.md')
    m = re.search(r'`kdos-release\.pub`\s*\|[^|\n]*key id `([0-9a-f]{16})`', read(path))
    if not m:
        die('%s: the download table names no release key id' % rel(path))
    return m.group(1)


def c_define(path, name, text=None):
    """The number a `#define NAME <n>` or `#define NAME <n>ULL` gives."""
    m = re.search(r'^#define\s+%s\s+(\d+)U?L{0,2}\s*$' % name, text or read(path), re.M)
    if not m:
        die('%s: no #define %s <number>' % (rel(path), name))
    return int(m.group(1))


def release_disk():
    """The disks an install needs: {two slots with the default swap, two
    slots with none, the default swap, one slot with none}, each a size that
    is enough. The two-slot sums are kinstall-core's own, ki_ab_need_mb()
    plus the ESP and the BIOS boot partition, with each slot at its
    KI_SLOT_MIN_MB floor: that holds while half again the install stays
    under the floor, as the book says it does for the base. ports/RELEASE,
    the release notes, must give the same sums to a tenth of a GiB, and the
    page rounds each up to a whole GiB and GB, so a disk of the size it
    names holds the slots. One slot rests on the install's own size, which
    only an image knows, so its figure is the release notes' own."""
    head = os.path.join(ROOT, 'src', 'system', 'kinstall-core', 'kinstall-core.h')
    conf = os.path.join(ROOT, 'src', 'system', 'kinstall-core', 'conf.c')
    k = {n: c_define(head, n) for n in ('KI_ESP_MB', 'KI_BIOS_BOOT_MB', 'KI_SLOT_MIN_MB',
                                        'KI_HOME_MIN_MB')}
    meta = c_define(conf, 'KI_VOLUME_META_MB')
    m = re.search(r'^\s*cfg\.swap_mb\s*=\s*(\d+);', read(conf), re.M)
    if not m:
        die('%s: no default cfg.swap_mb' % rel(conf))
    swap_mb = int(m.group(1))
    fixed = (meta + 2 * k['KI_SLOT_MIN_MB'] + k['KI_HOME_MIN_MB'] + k['KI_ESP_MB']
             + k['KI_BIOS_BOOT_MB'])
    need = (fixed + swap_mb, fixed)

    path = os.path.join(ROOT, 'ports', 'RELEASE')
    text = ' '.join(read(path).split())
    m = re.search(r'about ([\d.]+) GiB of disk .*? with the default ([\d.]+) GiB swap '
                  r'\(([\d.]+) GiB with none\)\. A smaller disk .*? down to about '
                  r'([\d.]+) GiB with no swap', text)
    if not m:
        die('%s: no "about N GiB of disk … default N GiB swap (N GiB with none). A smaller '
            'disk … down to about N GiB with no swap"' % rel(path))
    said = (m.group(1), m.group(3))
    for mb, gib in zip(need, said):
        if '%.1f' % (mb / 1024.0) != '%.1f' % float(gib):
            die('%s: says %s GiB where kinstall-core needs %d MiB (%.1f GiB)'
                % (rel(path), gib, mb, mb / 1024.0))
    if float(m.group(2)) * 1024 != swap_mb:
        die('%s: says a %s GiB swap where kinstall-core defaults to %d MiB'
            % (rel(path), m.group(2), swap_mb))

    def gib(mb):
        return unit('%d GiB' % -(-mb // 1024))

    def gb(mb):
        return unit('%d GB' % -(-(mb << 20) // 10 ** 9))

    return {'DISK': gib(need[0]), 'DISK_GB': gb(need[0]),
            'DISK_NOSWAP': gib(need[1]), 'DISK_NOSWAP_GB': gb(need[1]),
            'SWAP': unit('%s GiB' % m.group(2)), 'DISK_ONE': unit('%s GiB' % m.group(4))}


def book_chapters():
    text = read(os.path.join(BOOK, 'README.md'))
    m = re.search(r'^## Contents\n(.*?)^## ', text, re.S | re.M)
    n = len(re.findall(r'^\d+\. \[', m.group(1), re.M)) if m else 0
    if not n:
        die('docs/kdos/README.md: no numbered chapters under ## Contents')
    return n


def word(n):
    return WORDS[n] if n < len(WORDS) else '{:,}'.format(n)


def facts(schemes, default):
    osr = os_release()
    version = osr['VERSION_ID']
    tag = 'v' + version
    ports = len(glob.glob(os.path.join(ROOT, 'ports', 'core', '*', '*', 'kpkgbuild')))
    own = sum(len(glob.glob(os.path.join(ROOT, 'src', a, '*', 'kpkgbuild')))
              for a in ('system', 'art', 'desktop', 'daemons'))
    lic = own_licences()
    whole = sorted((n, spdx) for spdx, names in lic.items()
                   if 'MIT' not in re.findall(r'[\w.+-]+', spdx) for n in names)
    mixed = sorted([n for spdx, names in lic.items()
                    if 'MIT' in re.findall(r'[\w.+-]+', spdx) for n in names]
                   + bootstrap_mixed())
    if not whole or not mixed:
        die('src/*/*/kpkgbuild: site/home.md names KDOS programs under another licence '
            'and programs with other code beside MIT, and this tree has %d and %d: '
            'change its Licence words' % (len(whole), len(mixed)))
    kf = version_of('kcoreaddons')
    sources, dev_disk = build_disk()
    names = [s['title'] for s in schemes]
    f = {
        'VERSION': version,
        'TAG': tag,
        'PRETTY_NAME': osr['PRETTY_NAME'],
        'ISO': 'kdos-%s.iso' % tag,
        'RELEASE_URL': '%s/releases/tag/%s' % (REPO, tag),
        'DOWNLOAD': '%s/releases/download/%s' % (REPO, tag),
        'REPO': REPO,
        'ISSUES': REPO + '/issues',
        'SITE_URL': CANONICAL,
        'PORTS': '{:,}'.format(ports + own),
        'OWN': str(own),
        'AUTHOR': author(),
        'SHELVES': str(len(lines_of(os.path.join(ROOT, 'ports', 'shelves')))),
        'APPS': str(catalogue_apps()),
        'GROUPS': str(sum(1 for l in lines_of(os.path.join(ROOT, 'ports', 'groups'))
                          if l.split()[0] == 'group')),
        'LIBS': str(len(glob.glob(os.path.join(ROOT, 'src', 'libs', 'libk*', '')))),
        'CHAPTERS': word(book_chapters()),
        'PARTS': word(book_parts()),
        'PLASMA': version_of('plasma-workspace'),
        'KF': '.'.join(kf.split('.')[:2]),
        'GEAR': version_of('dolphin'),
        'QT': version_of('qt6-qtbase'),
        'LINUX': version_of('linux'),
        'MUSL': version_of('musl'),
        'SYSVINIT': version_of('sysvinit'),
        'FIREFOX': version_of('firefox-esr'),
        'ISO_LIMIT': iso_limit(),
        'KEYID': release_key_id(),
        'SOURCES_GB': sources,
        'DEV_DISK': dev_disk,
        'ACCENTS': word(len(schemes)),
        'ACCENTS_TITLE': word(len(schemes)).capitalize(),
        'ACCENT_NAMES': and_list(names),
        'DEFAULT_ACCENT': default.capitalize(),
        'LICENCE_OTHER': and_list(['`%s` (%s)' % w for w in whole]),
        'LICENCE_OTHER_N': word(len(whole)),
        'LICENCE_MIXED': and_list(['`%s`' % n for n in mixed]),
    }
    f.update(release_disk())
    return f


def substitute(text, values, where):
    def one(m):
        key = m.group(1)
        if key not in values:
            die('%s: {{%s}} is not a fact or slot this script knows' % (where, key))
        return values[key]
    return re.sub(r'\{\{\s*([A-Za-z_][A-Za-z0-9_]*)\s*\}\}', one, text)


# ── the manifests and the pictures ───────────────────────────────────────

def read_tsv(path):
    rows = [l.split('\t') for l in read(path).splitlines() if l.strip()]
    if not rows:
        die('%s: empty' % rel(path))
    head = rows[0]
    out = []
    for n, r in enumerate(rows[1:], 2):
        if len(r) != len(head):
            die('%s:%d: %d fields, the header has %d' % (rel(path), n, len(r), len(head)))
        out.append(dict(zip(head, r)))
    return out


def image_size(path):
    """(width, height) of a PNG or WebP, from its header."""
    with open(path, 'rb') as f:
        head = f.read(64)
    if head[:8] == b'\x89PNG\r\n\x1a\n':
        return struct.unpack('>II', head[16:24])
    if head[:4] == b'RIFF' and head[8:12] == b'WEBP':
        kind = head[12:16]
        if kind == b'VP8X':
            w = int.from_bytes(head[24:27], 'little') + 1
            h = int.from_bytes(head[27:30], 'little') + 1
            return w, h
        if kind == b'VP8 ':
            w, h = struct.unpack('<HH', head[26:30])
            return w & 0x3fff, h & 0x3fff
        if kind == b'VP8L':
            b = int.from_bytes(head[21:25], 'little')
            return (b & 0x3fff) + 1, ((b >> 14) & 0x3fff) + 1
    die('%s: not a PNG or WebP this script can measure' % rel(path))


def image_animated(path):
    """Whether a WebP is an animation: its VP8X header's animation flag."""
    with open(path, 'rb') as f:
        head = f.read(21)
    return head[:4] == b'RIFF' and head[8:16] == b'WEBPVP8X' and bool(head[20] & 0x02)


class Assets:
    def __init__(self):
        self.art = {r['file']: r for r in read_tsv(os.path.join(ASSETS, 'art', 'art.tsv'))}
        rows = read_tsv(os.path.join(ASSETS, 'shots', 'shots.tsv'))
        self.shot_order = [r['file'][:-len('.webp')] for r in rows]
        self.shots = {r['file'][:-len('.webp')]: r for r in rows}
        self.sizes = {}

    def size(self, relpath):
        if relpath not in self.sizes:
            path = os.path.join(ASSETS, relpath)
            if not os.path.isfile(path):
                die('site/assets/%s: no such file' % relpath)
            self.sizes[relpath] = image_size(path)
        return self.sizes[relpath]

    def animated(self, relpath):
        self.size(relpath)
        return image_animated(os.path.join(ASSETS, relpath))

    def art_alt(self, name):
        if name not in self.art:
            die('site/assets/art/%s: not in art.tsv' % name)
        return self.art[name]['alt']

    def shot(self, name):
        if name not in self.shots:
            die('shot %s: not in site/assets/shots/shots.tsv' % name)
        return self.shots[name]


# ── headings and ids, as GitHub gives them ───────────────────────────────

LINK_RE = re.compile(r'(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(([^()\s]+)\)')
CODE_RE = re.compile(r'(`+)(.+?)(?<!`)\1(?!`)')
FENCE_RE = re.compile(r'^\s*(```|~~~)')
HEAD_RE = re.compile(r'^(#{1,6})\s+(.*?)\s*#*\s*$')


def plain(md):
    md = LINK_RE.sub(lambda m: m.group(2), md)
    return md.replace('`', '').replace('**', '').replace('*', '')


def anchor(text):
    """The id GitHub gives a heading; kdos-doc's mkbook.py and
    testing/anchorcheck.py use the same rule."""
    s = plain(text).strip().lower()
    s = re.sub(r'[^\w\- ]', '', s)
    return s.replace(' ', '-')


class Ids:
    def __init__(self):
        self.seen = {}

    def take(self, text):
        a = anchor(text)
        n = self.seen.get(a, 0)
        self.seen[a] = n + 1
        return a if not n else '%s-%d' % (a, n)


_page_ids = {}


def page_ids(path):
    """Every heading id of a Markdown page, and its explicit <a id> anchors."""
    if path not in _page_ids:
        ids, fence = Ids(), False
        out = set()
        text = read(path)
        for line in text.split('\n'):
            if FENCE_RE.match(line):
                fence = not fence
                continue
            m = None if fence else HEAD_RE.match(line)
            if m:
                out.add(ids.take(m.group(2)))
        out.update(re.findall(r'<a\s+(?:id|name)="([^"]+)"', text))
        _page_ids[path] = out
    return _page_ids[path]


# ── links ────────────────────────────────────────────────────────────────

class Linker:
    """A target in home.md as one output needs it, checked once."""

    def __init__(self, fmt, local_ids):
        self.fmt = fmt
        self.local_ids = local_ids

    def __call__(self, target):
        path, _, frag = target.partition('#')
        hashed = ('#' + frag) if frag else ''
        if target.startswith('#'):
            if frag not in self.local_ids:
                die('link %s: no heading of the page has that id' % target)
            return target
        if re.match(r'(https?|mailto):', target):
            return target
        if path.startswith('book:'):
            page = path[len('book:'):] or 'README.md'
            src = os.path.join(BOOK, page)
            if not page.endswith('.md') or not os.path.isfile(src):
                die('link %s: docs/kdos/%s is not a page of the book' % (target, page))
            if frag and frag not in page_ids(src):
                die('link %s: docs/kdos/%s has no heading #%s' % (target, page, frag))
            if self.fmt == 'readme':
                return 'docs/kdos/' + page + hashed
            out = '' if page == 'README.md' else page[:-3] + '.html'
            return 'book/' + out + hashed
        if path in ('kfs:', 'kfs:kfs.pdf'):
            # The rendered book, from either output: docs/kfs on GitHub has
            # no package pages, since they are made from the recipes when
            # the book is rendered, and no PDF.
            if frag:
                die('link %s: KDOS from Scratch is linked at its contents only' % target)
            out = 'kfs/' + ('kfs.pdf' if path == 'kfs:kfs.pdf' else '')
            return (CANONICAL + out) if self.fmt == 'readme' else out
        if path.startswith('repo:'):
            p = path[len('repo:'):]
            full = os.path.join(ROOT, p)
            if not os.path.exists(full):
                die('link %s: %s does not exist' % (target, p))
            if self.fmt == 'readme':
                return p + hashed
            return (TREE if os.path.isdir(full) else BLOB) + p + hashed
        if path.startswith('asset:'):
            p = path[len('asset:'):]
            if not os.path.isfile(os.path.join(ASSETS, p)):
                die('link %s: site/assets/%s does not exist' % (target, p))
            return ('site/assets/' if self.fmt == 'readme' else 'assets/') + p + hashed
        die('link %s: a target is #id, http(s):, mailto:, book:, kfs:, repo: or asset:'
            % target)


NOWRAP_CODE = 28


def esc(s):
    """Text for HTML, an attribute value in double quotes included."""
    return html.escape(s, quote=False).replace('"', '&quot;')


def inline_html(text, link, nowrap=False):
    """Inline Markdown as HTML: code spans, links and images, bold, em; with
    NOWRAP, code spans that keep their words whole, for the homepage."""
    stash = []

    def keep(s):
        stash.append(s)
        return '\x00%d\x00' % (len(stash) - 1)

    # A browser breaks a line after a hyphen, which splits `kdos-doc` into
    # two words that read as two commands. A short span is kept on one line
    # whole, and a longer one breaks only at its spaces: NOWRAP_CODE
    # characters of code fit a 360-pixel screen.
    def code(m):
        body = m.group(2)
        if len(body) > 2 and body[0] == ' ' and body[-1] == ' ':
            body = body[1:-1]
        if not nowrap:
            return keep('<code>%s</code>' % esc(body))
        if len(body) <= NOWRAP_CODE:
            return keep('<code class="nw">%s</code>' % esc(body))
        words = re.split(r'(\s+)', body)
        return keep('<code>%s</code>' % ''.join(
            '<span class="nw">%s</span>' % esc(w) if '-' in w and len(w) <= NOWRAP_CODE
            else esc(w) for w in words))

    def emph(s):
        s = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', s)
        return re.sub(r'(?<![\w*])\*(?=\S)(.+?)(?<=\S)\*(?![\w*])', r'<em>\1</em>', s)

    def anchor_tag(m):
        bang, label, target = m.group(1), m.group(2), html.unescape(m.group(3))
        url = esc(link(target))
        if bang:
            return keep('<img src="%s" alt="%s">' % (url, label))
        ext = ' rel="noopener"' if url.startswith('http') else ''
        return keep('<a href="%s"%s>%s</a>' % (url, ext, emph(label)))

    text = CODE_RE.sub(code, text)
    text = esc(text)
    text = LINK_RE.sub(anchor_tag, text)
    text = emph(text)
    while '\x00' in text:
        text = re.sub(r'\x00(\d+)\x00', lambda m: stash[int(m.group(1))], text)
    return text


def inline_md(text, link):
    """Inline Markdown as README Markdown: the same text, targets mapped."""
    stash = []

    def keep(m):
        stash.append(m.group(0))
        return '\x00%d\x00' % (len(stash) - 1)

    text = CODE_RE.sub(keep, text)
    text = LINK_RE.sub(lambda m: '%s[%s](%s)' % (m.group(1), m.group(2), link(m.group(3))),
                       text)
    return re.sub(r'\x00(\d+)\x00', lambda m: stash[int(m.group(1))], text)


# ── blocks ───────────────────────────────────────────────────────────────

LIST_RE = re.compile(r'^(\s*)([-*]|\d+\.)\s+(.*)$')
TABLE_SEP_RE = re.compile(r'^\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)*\|?\s*$')
DIRECTIVES = ('hero', 'badges', 'facts', 'terminal', 'gallery', 'accents', 'cards',
              'steps', 'note', 'html', 'readme', 'js')
ACCENT_KEYS = ('default-shot', 'default-caption', 'crt-shot')


def cells(line):
    line = line.strip()
    if line.startswith('|'):
        line = line[1:]
    if line.endswith('|') and not line.endswith('\\|'):
        line = line[:-1]
    return [c.strip().replace('\\|', '|') for c in re.split(r'(?<!\\)\|', line)]


def starts_block(line):
    return (line.startswith('#') or line.startswith('```') or line.startswith(':::')
            or line.startswith('|') or LIST_RE.match(line) is not None
            or line.startswith('<!--'))


def parse(lines, where):
    """[(kind, …)] for a run of Markdown lines."""
    out, i = [], 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
        elif line.startswith('<!--'):
            while '-->' not in lines[i]:
                i += 1
                if i == len(lines):
                    die('%s: a comment is never closed' % where)
            i += 1
        elif line.startswith(':::'):
            words = line[3:].split()
            if not words or words[0] not in DIRECTIVES:
                die('%s: `%s` is not a directive (%s)' % (where, line, ', '.join(DIRECTIVES)))
            j = i + 1
            while j < len(lines) and lines[j].strip() != ':::':
                if lines[j].startswith(':::'):
                    die('%s: ::: %s holds another directive' % (where, words[0]))
                j += 1
            if j == len(lines):
                die('%s: ::: %s is never closed' % (where, words[0]))
            out.append(('directive', words[0], lines[i + 1:j]))
            i = j + 1
        elif line.startswith('```'):
            lang = line[3:].strip()
            j = i + 1
            while j < len(lines) and not lines[j].startswith('```'):
                j += 1
            if j == len(lines):
                die('%s: a code block is never closed' % where)
            out.append(('code', lang, '\n'.join(lines[i + 1:j])))
            i = j + 1
        elif HEAD_RE.match(line) and line.startswith('#'):
            m = HEAD_RE.match(line)
            out.append(('heading', len(m.group(1)), m.group(2)))
            i += 1
        elif line.startswith('|') and i + 1 < len(lines) and TABLE_SEP_RE.match(lines[i + 1]):
            head = cells(line)
            j, rows = i + 2, []
            while j < len(lines) and lines[j].startswith('|'):
                row = cells(lines[j])
                if len(row) != len(head):
                    die('%s: a table row has %d cells, its header %d' % (where, len(row), len(head)))
                rows.append(row)
                j += 1
            out.append(('table', head, rows))
            i = j
        elif LIST_RE.match(line):
            ordered = LIST_RE.match(line).group(2)[0].isdigit()
            items = []
            while i < len(lines):
                m = LIST_RE.match(lines[i])
                if m and m.group(2)[0].isdigit() == ordered:
                    items.append(m.group(3))
                elif lines[i].startswith('  ') and lines[i].strip() and items:
                    items[-1] += ' ' + lines[i].strip()
                else:
                    break
                i += 1
            out.append(('list', ordered, items))
        else:
            para = []
            while i < len(lines) and lines[i].strip() and (not para or not starts_block(lines[i])):
                para.append(lines[i].strip())
                i += 1
            out.append(('para', ' '.join(para)))
    return out


def keyvals(body, allowed, where):
    out = []
    for line in body:
        if not line.strip():
            continue
        m = re.match(r'([a-z-]+):\s*(.*)$', line.strip())
        if not m or m.group(1) not in allowed:
            die('%s: `%s` is not one of %s' % (where, line.strip(), ', '.join(allowed)))
        out.append((m.group(1), m.group(2)))
    return out


def split_titled(blocks, where, icon=False):
    """[(title, icon, [blocks])] for a body of `### Title` runs."""
    out = []
    for b in blocks:
        if b[0] == 'heading' and b[1] == 3:
            out.append([b[2], None, []])
        elif not out:
            die('%s: text before the first ### heading' % where)
        elif icon and b[0] == 'para' and b[1].startswith('icon:') and not out[-1][2]:
            name, _, rest = b[1][len('icon:'):].strip().partition(' ')
            out[-1][1] = name
            if rest.strip():
                out[-1][2].append(('para', rest.strip()))
        else:
            out[-1][2].append(b)
    if not out:
        die('%s: no ### headings' % where)
    return out


# ── the page model ───────────────────────────────────────────────────────

class Page:
    def __init__(self, values, assets, schemes, default, pairs):
        text = re.sub(r'^<!--.*?-->[ \t]*$', '', read(HOME), flags=re.S | re.M)
        text = substitute(text, values, 'site/home.md')
        lines = text.split('\n')
        self.meta = {}
        if lines and lines[0].strip() == '---':
            end = lines.index('---', 1)
            for line in lines[1:end]:
                k, _, v = line.partition(':')
                self.meta[k.strip()] = v.strip()
            lines = lines[end + 1:]
        for key in ('title', 'description', 'nav'):
            if not self.meta.get(key):
                die('site/home.md: the front matter has no %s' % key)
        self.blocks = parse(lines, 'site/home.md')
        self.values, self.assets = values, assets
        self.schemes, self.default, self.pairs = schemes, default, pairs
        self.ids = self.heading_ids()
        self.check_shots()

    def walk(self, fmt):
        """Every block in document order, format-only directives opened for
        FMT, other directives' bodies parsed."""
        for b in self.blocks:
            if b[0] == 'directive' and b[1] in ('html', 'readme'):
                if b[1] == fmt:
                    yield from parse(b[2], 'site/home.md ::: ' + b[1])
            else:
                yield b

    def heading_ids(self):
        ids = set()
        seq = Ids()
        for b in self.walk('html'):
            if b[0] == 'heading':
                ids.add(seq.take(b[2]))
            elif b[0] == 'directive' and b[1] in ('cards', 'steps'):
                for t in split_titled(parse(b[2], b[1]), b[1]):
                    ids.add(seq.take(t[0]))
            elif b[0] == 'directive' and b[1] == 'gallery':
                for title, _ in self.gallery_groups(b[2]):
                    ids.add(seq.take(title))
        return ids

    def gallery_groups(self, body):
        groups = []
        for line in body:
            if not line.strip() or line.startswith('colour:'):
                continue
            if line.startswith('group:'):
                groups.append((line[len('group:'):].strip(), []))
            elif not groups:
                die('::: gallery: shots before the first group:')
            else:
                for name in line.split():
                    self.assets.shot(name)
                    groups[-1][1].append(name)
        return groups

    def colour_shots(self):
        """The shots a gallery's `colour:` lines name, each one it shows."""
        out = set()
        for b in self.blocks:
            if b[0] == 'directive' and b[1] == 'gallery':
                shown = {n for _, names in self.gallery_groups(b[2]) for n in names}
                for line in b[2]:
                    if line.startswith('colour:'):
                        for name in line[len('colour:'):].split():
                            if name not in shown:
                                die('::: gallery: colour: names %s, which it does not show' % name)
                            out.add(name)
        return out

    def check_shots(self):
        shown = set()
        for b in self.blocks:
            if b[0] == 'directive' and b[1] == 'gallery':
                for _, names in self.gallery_groups(b[2]):
                    shown.update(names)
            if b[0] == 'directive' and b[1] == 'accents':
                shown.update(self.accent_shots(b[2]).values())
        missing = [n for n in self.assets.shot_order if n not in shown]
        if missing:
            die('site/home.md: shots.tsv lists pictures no gallery or accent picker shows: '
                + ', '.join(missing))

    def accent_shots(self, body):
        opts = dict(keyvals(body, ACCENT_KEYS, '::: accents'))
        out = {}
        for s in self.schemes:
            name = 'accent-' + s['name']
            if name in self.assets.shots:
                out[s['name']] = name
            elif s['name'] == self.default and opts.get('default-shot'):
                out[s['name']] = opts['default-shot']
            else:
                die('::: accents: no shot of the %s accent, and no default-shot: for it'
                    % s['name'])
            self.assets.shot(out[s['name']])
        if opts.get('crt-shot'):
            self.assets.shot(opts['crt-shot'])
            out['crt'] = opts['crt-shot']
        return out

    def accent_caption(self, body):
        """The words under the default accent's picture: default-caption:, or
        that shot's own caption when there is none."""
        opts = dict(keyvals(body, ACCENT_KEYS, '::: accents'))
        return opts.get('default-caption') or \
            self.assets.shot(self.accent_shots(body)[self.default])['caption']


# ── README.md ────────────────────────────────────────────────────────────

class Readme:
    def __init__(self, page):
        self.p = page
        self.link = Linker('readme', page.ids)

    def inl(self, s):
        return inline_md(s, self.link)

    def html_inl(self, s):
        return inline_html(s, self.link)

    def img(self, src, alt, width=None, height=None, extra=''):
        size = ''
        if width:
            size += ' width="%s"' % width
        if height:
            size += ' height="%s"' % height
        return '<img src="%s" alt="%s"%s%s>' % (esc(src), esc(alt), size, extra)

    def blocks(self, blocks):
        out = []
        for b in blocks:
            out.append(self.block(b))
        return [x for x in out if x is not None]

    def block(self, b):
        kind = b[0]
        if kind == 'heading':
            return '#' * b[1] + ' ' + self.inl(b[2])
        if kind == 'para':
            return self.inl(b[1])
        if kind == 'list':
            lines = []
            for n, item in enumerate(b[2], 1):
                lines.append(('%d. ' % n if b[1] else '- ') + self.inl(item))
            return '\n'.join(lines)
        if kind == 'code':
            return '```%s\n%s\n```' % (b[1], b[2])
        if kind == 'table':
            return self.table(b[1], b[2])
        if kind == 'directive':
            return getattr(self, 'd_' + b[1])(b[2])
        die('README: no rendering for %s' % kind)

    def table(self, head, rows):
        def row(cs):
            return '| ' + ' | '.join(self.inl(c).replace('|', '\\|') for c in cs) + ' |'
        return '\n'.join([row(head), '|' + '---|' * len(head)] + [row(r) for r in rows])

    def d_hero(self, body):
        kv = keyvals(body, ('subtitle', 'pitch', 'button', 'note'), '::: hero')
        get = dict(kv)
        hero = 'readme-hero.png'
        w, h = self.p.assets.size('art/' + hero)
        # The README adds the homepage's own address to the buttons: a reader
        # on GitHub is the one who has not seen it.
        buttons = ' · '.join(['<a href="%s"><b>%s</b></a>'
                              % (esc(self.link(t.strip())), esc(l.strip()))
                              for l, _, t in (v.partition('|') for k, v in kv if k == 'button')]
                             + ['<a href="%s"><b>%s</b></a>' % (CANONICAL,
                                                                CANONICAL[8:].rstrip('/'))])
        # The hero picture already says the subtitle, so the README gives
        # the pitch alone under it.
        out = ['<p align="center">\n  <a href="%s">%s</a>\n</p>'
               % (CANONICAL, self.img('site/assets/art/' + hero,
                                      self.p.assets.art_alt(hero), '100%')),
               '<p align="center">\n  %s\n</p>' % self.html_inl(get['pitch']),
               '<p align="center">\n  %s\n</p>' % buttons]
        if get.get('note'):
            out.append('<p align="center"><sub>%s</sub></p>' % self.html_inl(get['note']))
        return '\n\n'.join(out)

    def d_badges(self, body):
        imgs = []
        for name in ' '.join(body).split():
            f = 'badge-%s.png' % name
            w, h = self.p.assets.size('art/' + f)
            imgs.append('  ' + self.img('site/assets/art/' + f, self.p.assets.art_alt(f), w, h))
        return '<p align="center">\n%s\n</p>' % '\n'.join(imgs)

    def d_facts(self, body):
        blocks = parse(body, '::: facts')
        if len(blocks) != 1 or blocks[0][0] != 'table':
            die('::: facts: holds one table and nothing else')
        _, head, rows = blocks[0]
        if any(h.strip() for h in head):
            die('::: facts: the header row is empty; each row is a label and its value')
        return '<table>\n%s\n</table>' % '\n'.join(
            '<tr><th align="left">%s</th><td>%s</td></tr>'
            % (self.html_inl(r[0]), ' '.join(self.html_inl(c) for c in r[1:]))
            for r in rows)

    def d_terminal(self, body):
        return None

    def d_js(self, body):
        return None

    # GitHub shows a README with no script, so each picture carries its
    # caption, which the homepage's lightbox shows, under it; and each
    # group takes the column count that fills its rows. The README has no
    # accent switcher to show an accent's own picture in, so that picture
    # is in its card of the accents table, which opens it, and not in the
    # gallery a second time.
    @staticmethod
    def gallery_cols(n):
        for cols in (3, 4, 2, 5):
            if n % cols == 0:
                return cols
        return 3

    def gallery_table(self, names):
        rows, cols = [], self.gallery_cols(len(names))
        for i in range(0, len(names), cols):
            cells_ = []
            for name in names[i:i + cols]:
                s = self.p.assets.shot(name)
                cells_.append(
                    '<td align="center" valign="top" width="%d%%">'
                    '<a href="site/assets/shots/%s">%s</a><br><sub>%s</sub></td>'
                    % (100 // cols, esc(s['file']),
                       self.img('site/assets/shots/' + s['thumb'], s['alt'], '100%'),
                       esc(s['caption'])))
            rows.append('<tr>\n' + '\n'.join(cells_) + '\n</tr>')
        return '<table>\n' + '\n'.join(rows) + '\n</table>'

    def d_gallery(self, body):
        carded = set()
        for b in self.p.blocks:
            if b[0] == 'directive' and b[1] == 'accents':
                carded.update(n for k, n in self.p.accent_shots(b[2]).items()
                              if n == 'accent-' + k)
        out = []
        for title, names in self.p.gallery_groups(body):
            names = [n for n in names if n not in carded]
            if names:
                out.append('### ' + title)
                out.append(self.gallery_table(names))
        return '\n\n'.join(out)

    def d_accents(self, body):
        shots = self.p.accent_shots(body)
        cols, rows = 4, []
        for i in range(0, len(self.p.schemes), cols):
            tds = []
            for s in self.p.schemes[i:i + cols]:
                card = 'accent-%s.png' % s['name']
                shot = self.p.assets.shot(shots[s['name']])
                label = s['title'] + (' (default)' if s['name'] == self.p.default else '')
                tds.append('<td align="center" width="25%%"><a href="site/assets/shots/%s">%s</a>'
                           '<br><sub><b>%s</b></sub></td>'
                           % (esc(shot['file']),
                              self.img('site/assets/art/' + card,
                                       self.p.assets.art_alt(card), '100%'),
                              esc(label)))
            rows.append('<tr>\n' + '\n'.join(tds) + '\n</tr>')
        return '<table>\n' + '\n'.join(rows) + '\n</table>'

    def d_cards(self, body):
        out = []
        for title, _, blocks in split_titled(parse(body, '::: cards'), '::: cards', icon=True):
            out.append('### ' + self.inl(title))
            out.extend(self.blocks(blocks))
        return '\n\n'.join(out)

    def d_steps(self, body):
        out = []
        for title, _, blocks in split_titled(parse(body, '::: steps'), '::: steps'):
            out.append('### ' + self.inl(title))
            out.extend(self.blocks(blocks))
        return '\n\n'.join(out)

    def d_note(self, body):
        text = '\n\n'.join(self.blocks(parse(body, '::: note')))
        return '\n'.join(['> [!NOTE]'] + ['> ' + l if l else '>' for l in text.split('\n')])

    def render(self):
        head = ('<!-- This file is written by script/site/mksite.py from site/home.md, '
                'which kdos.dev is made from as well.\n'
                '     Edit site/home.md and run `make site`; `make sitecheck` fails while '
                'the two differ. -->')
        body = self.blocks(list(self.p.walk('readme')))
        return head + '\n\n' + '\n\n'.join(body).rstrip() + '\n'


# ── the homepage ─────────────────────────────────────────────────────────

class Homepage:
    def __init__(self, page):
        self.p = page
        self.link = Linker('site', page.ids)
        self.ids = Ids()
        self.preload = []
        self.gallery_index = 0
        self.colour_shots = {'accent-' + sc['name'] for sc in page.schemes} | page.colour_shots()

    def inl(self, s):
        return inline_html(s, self.link, nowrap=True)

    def img(self, src, alt, size, cls='', lazy=True, extra=''):
        w, h = size
        attrs = ' class="%s"' % cls if cls else ''
        attrs += ' loading="lazy" decoding="async"' if lazy else ''
        return '<img src="%s" alt="%s" width="%d" height="%d"%s%s>' % (
            esc(src), esc(alt), w, h, attrs, extra)

    def blocks(self, blocks):
        return '\n'.join(x for x in (self.block(b) for b in blocks) if x)

    def block(self, b):
        kind = b[0]
        if kind == 'heading':
            hid = self.ids.take(b[2])
            return '<h%d id="%s">%s</h%d>' % (b[1], hid, self.inl(b[2]), b[1])
        if kind == 'para':
            return '<p>%s</p>' % self.inl(b[1])
        if kind == 'list':
            tag = 'ol' if b[1] else 'ul'
            return '<%s>\n%s\n</%s>' % (tag, '\n'.join('<li>%s</li>' % self.inl(i)
                                                     for i in b[2]), tag)
        if kind == 'code':
            # A pane wider than a phone scrolls sideways, and only a pane in
            # the tab order can be scrolled from the keyboard.
            cls = ' class="language-%s"' % esc(b[1]) if b[1] else ''
            return ('<div class="code"><pre tabindex="0"><code%s>%s</code></pre></div>'
                    % (cls, esc(b[2])))
        if kind == 'table':
            return self.table(b[1], b[2])
        if kind == 'directive':
            return getattr(self, 'd_' + b[1])(b[2])
        die('homepage: no rendering for %s' % kind)

    def table(self, head, rows, cls='table'):
        out = ['<div class="table-wrap"><table class="%s">' % cls]
        if any(h.strip() for h in head):
            out.append('<thead><tr>%s</tr></thead>'
                       % ''.join('<th scope="col">%s</th>' % self.inl(h) for h in head))
        out.append('<tbody>')
        labels = [plain(h).strip() for h in head]
        for r in rows:
            tds = ''.join('<td%s>%s</td>' % (' data-label="%s"' % esc(l) if l else '', self.inl(c))
                          for l, c in zip(labels[1:], r[1:]))
            out.append('<tr><th scope="row">%s</th>%s</tr>' % (self.inl(r[0]), tds))
        out.append('</tbody></table></div>')
        return '\n'.join(out)

    # The accents' own pictures follow the accent: site.js swaps the file
    # named by data-accent-src, {accent} replaced, and the page shows the
    # default's until then.
    def accent_img(self, stem, cls, lazy, alt=None, extra=''):
        f = '%s-%s.png' % (stem, self.p.default)
        for s in self.p.schemes:
            self.p.assets.size('art/%s-%s.png' % (stem, s['name']))
        return self.img('assets/art/' + f, self.p.assets.art_alt(f) if alt is None else alt,
                        self.p.assets.size('art/' + f), cls, lazy,
                        ' data-accent-src="assets/art/%s-{accent}.png"%s' % (stem, extra))

    # The hero's two are fetched once, already in the reader's accent: the
    # <img> holds an empty picture until HERO_SCRIPT, run as soon as the
    # hero is parsed, names the accent's file, so neither the default's file
    # is fetched nor its colours painted first. The head script has started
    # that file's download at high priority already (self.preload), so it
    # does not wait for the stylesheet that HERO_SCRIPT waits for. A reader
    # without script gets the default's from <noscript>, with EXTRA.
    def hero_img(self, stem, cls, extra=''):
        f = '%s-%s.png' % (stem, self.p.default)
        for s in self.p.schemes:
            self.p.assets.size('art/%s-%s.png' % (stem, s['name']))
        w, h = self.p.assets.size('art/' + f)
        alt = self.p.assets.art_alt(f)
        self.preload.append('assets/art/' + stem)
        return ('<img class="%s js-only" src="%s" alt="%s" width="%d" height="%d" '
                'data-accent-src="assets/art/%s-{accent}.png">'
                '<noscript>%s</noscript>'
                % (cls, EMPTY_GIF, esc(alt), w, h, stem,
                   self.img('assets/art/' + f, alt, (w, h), cls, False, extra)))

    def switcher(self):
        btns = []
        for s in self.p.schemes:
            h = s['hex']
            btns.append(
                '<button type="button" class="swatch" data-accent-set="%s" aria-pressed="%s" '
                'title="%s" style="--sw-pri:#%s;--sw-sec:#%s;--sw-deep:#%s">'
                '<span class="sr-only">%s</span></button>'
                % (s['name'], 'true' if s['name'] == self.p.default else 'false',
                   esc(s['title']), h['primary'], h['secondary'], h['deep'], esc(s['title'])))
        return ('<div class="look js-only" role="group" aria-label="Accent">'
                '<span class="look__label" aria-hidden="true">Accent</span>'
                '<span class="look__swatches">%s</span>'
                '<span class="look__name" data-accent-title aria-live="polite">%s</span>'
                '<span class="look__via" data-accent-via></span></div>'
                % (''.join(btns), esc(self.p.default.capitalize())))

    def d_hero(self, body):
        kv = keyvals(body, ('subtitle', 'pitch', 'button', 'note'), '::: hero')
        get = dict(kv)
        buttons = []
        for n, (k, v) in enumerate((k, v) for k, v in kv if k == 'button'):
            label, _, target = v.partition('|')
            buttons.append('<a class="btn%s" href="%s">%s</a>'
                           % (' btn--primary' if not n else '', esc(self.link(target.strip())),
                              esc(label.strip())))
        note = '<p class="hero__note">%s</p>' % self.inl(get['note']) if get.get('note') else ''
        return '\n'.join([
            '<section class="hero" aria-labelledby="hero-title">',
            '<div class="hero__inner wrap">',
            '<div class="hero__text">',
            '<h1 id="hero-title" class="hero__title">%s</h1>'
            % self.hero_img('banner', 'hero__banner', extra=' fetchpriority="high"'),
            '<p class="hero__subtitle">%s</p>' % esc(get['subtitle']),
            '<p class="hero__pitch">%s</p>' % self.inl(get['pitch']),
            '<div class="hero__actions">%s</div>' % ''.join(buttons),
            note,
            self.switcher(),
            '</div>',
            '<div class="hero__art">%s</div>' % self.hero_img('penguin', 'hero__penguin'),
            '</div>',
            '</section>',
            '<script>%s</script>' % HERO_SCRIPT])

    def d_badges(self, body):
        imgs = []
        for name in ' '.join(body).split():
            f = 'badge-%s.png' % name
            imgs.append('<li>%s</li>' % self.img('assets/art/' + f, self.p.assets.art_alt(f),
                                                 self.p.assets.size('art/' + f), 'badge'))
        return '<ul class="badges" aria-label="Built with">%s</ul>' % ''.join(imgs)

    def d_facts(self, body):
        blocks = parse(body, '::: facts')
        if len(blocks) != 1 or blocks[0][0] != 'table':
            die('::: facts: holds one table and nothing else')
        return self.table(blocks[0][1], blocks[0][2], 'facts')

    def d_terminal(self, body):
        v, prompt = self.p.values, '<span class="term__prompt">kdos@kdos:~ λ</span> '
        out = []
        for cmd in (l.strip() for l in body if l.strip()):
            out.append(prompt + esc(cmd))
            if cmd == 'kdos version':
                out.append('<b>KDOS</b> — Kool Desktop Operating System')
                for k, val in (('release', v['VERSION']), ('kernel', v['LINUX']),
                               ('libc', 'musl'), ('userland', 'GNU'),
                               ('session', 'Plasma %s (Wayland)' % v['PLASMA'])):
                    out.append('  %-8s %s' % (k, esc(val)))
                out.append('  accent   <span data-accent-name>%s</span>' % self.p.default)
            elif cmd == 'kdos theme list':
                for s in self.p.schemes:
                    mark = '*' if s['name'] == self.p.default else ' '
                    out.append('<span data-theme-row="%s">%s %-10s %s</span>'
                               % (s['name'], mark, s['name'], esc(s['theme'])))
            else:
                die('::: terminal: no output is known for `%s`' % cmd)
        out.append(prompt + '<span class="term__cursor" aria-hidden="true"></span>')
        return ('<figure class="term" aria-label="A terminal on KDOS">'
                '<div class="term__bar" aria-hidden="true"><span>~ : bash — Konsole</span></div>'
                '<pre class="term__body" tabindex="0">%s</pre></figure>' % '\n'.join(out))

    # An animation opens in the lightbox as its still, <name>-still.webp at
    # the animation's own size, and plays only when the reader asks
    # (data-still): a picture that moves by itself for more than five
    # seconds needs a way to stop it, and it is the one large file of the
    # gallery. The accents' own pictures, and those the gallery's `colour:`
    # names, keep their colours in Phosphor mode (shot__img--colour), since
    # their colours are what they show.
    def shot_link(self, name, featured=False):
        s = self.p.assets.shot(name)
        full = 'shots/' + s['file']
        fw, fh = self.p.assets.size(full)
        thumb = self.p.assets.size('shots/' + s['thumb'])
        extra = ''
        if featured:
            extra = (' srcset="assets/shots/%s %dw, assets/%s %dw" '
                     'sizes="(min-width: 1041px) 590px, 100vw"'
                     % (esc(s['thumb']), thumb[0], esc(full), fw))
        anim = ''
        if self.p.assets.animated(full):
            still = s['file'][:-len('.webp')] + '-still.webp'
            if self.p.assets.size('shots/' + still) != (fw, fh):
                die('site/assets/shots/%s: not the size of %s' % (still, s['file']))
            anim = ' data-still="assets/shots/%s"' % esc(still)
        cls = 'shot__img' + (' shot__img--colour' if name in self.colour_shots else '')
        i = self.gallery_index
        self.gallery_index += 1
        return ('<a class="shot" href="assets/%s" data-index="%d" data-w="%d" data-h="%d" '
                'data-title="%s" data-caption="%s"%s>%s</a>'
                % (esc(full), i, fw, fh, esc(s['title']), esc(s['caption']), anim,
                   self.img('assets/shots/' + s['thumb'], s['alt'], thumb, cls,
                            extra=extra)))

    @staticmethod
    def grid_shape(n):
        """(columns, featured) that fills every row of a group of N on a wide
        screen: a featured first picture spans two columns and two rows, so
        it takes the room of four. Four columns first, then three, then
        five; a group nothing fills takes four and leaves its last row short."""
        for cols in (4, 3, 5):
            for featured in (False, True):
                if (n + (3 if featured else 0)) % cols == 0 and n >= (2 if featured else 1):
                    return cols, featured
        return 4, False

    def d_gallery(self, body):
        out = ['<div class="gallery" data-gallery>']
        for title, names in self.p.gallery_groups(body):
            hid = self.ids.take(title)
            cols, featured = self.grid_shape(len(names))
            out.append('<h3 id="%s" class="gallery__group">%s</h3>' % (hid, esc(title)))
            out.append('<ul class="gallery__grid" data-cols="%d">' % cols)
            for i, name in enumerate(names):
                s = self.p.assets.shot(name)
                cls = ' class="is-featured"' if featured and i == 0 else ''
                out.append('<li%s><figure>%s<figcaption>%s</figcaption></figure></li>'
                           % (cls, self.shot_link(name, bool(cls)), esc(s['title'])))
            out.append('</ul>')
        out.append('</div>')
        return '\n'.join(out)

    def d_accents(self, body):
        shots = self.p.accent_shots(body)
        data = {}
        for key, name in shots.items():
            s = self.p.assets.shot(name)
            w, h = self.p.assets.size('shots/' + s['file'])
            data[key] = {'src': 'assets/shots/' + s['file'], 'alt': s['alt'],
                         'caption': s['caption'], 'w': w, 'h': h}
        data[self.p.default]['caption'] = self.p.accent_caption(body)
        first = data[self.p.default]
        bezel = 'crt-bezel.png'
        bw, bh = self.p.assets.size('art/' + bezel)
        m = re.search(r'left ([\d.]+) top ([\d.]+) width ([\d.]+) height ([\d.]+)',
                      self.p.assets.art.get(bezel, {}).get('purpose', ''))
        if not m:
            die('art.tsv: %s gives no screen rectangle (left top width height, in %%)' % bezel)
        screen = 'left:%s%%;top:%s%%;width:%s%%;height:%s%%' % m.groups()
        cards = []
        for s in self.p.schemes:
            h = s['hex']
            style = ';'.join('--c-%s:#%s' % (CSS_VAR[k], h[k]) for k in KCOL_FIELDS)
            default = s['name'] == self.p.default
            cards.append(
                '<li><a class="acard" href="%s" data-accent-set="%s" style="%s">'
                '<span class="acard__tab">%s</span>'
                '<span class="acard__theme">%s%s</span>'
                '<span class="acard__chips" aria-hidden="true">'
                '<i style="background:var(--c-pri)"></i><i style="background:var(--c-sec)"></i>'
                '<i style="background:var(--c-urg)"></i></span>'
                '<span class="acard__cmd"><span aria-hidden="true">$ </span>kdos theme %s</span>'
                '</a></li>'
                % (esc(data[s['name']]['src']), s['name'], style, esc(s['label']),
                   esc(s['theme']), ' <small>default</small>' if default else '', s['name']))
        crt = ''
        if 'crt' in data:
            crt = ('<p class="accents__crt js-only"><button type="button" class="btn" '
                   'data-phosphor-toggle aria-pressed="false">Phosphor mode</button></p>')
        return '\n'.join([
            '<div class="accents" data-accent-shots="%s">' % esc(json.dumps(data, sort_keys=True)),
            '<figure class="monitor">',
            '<div class="monitor__frame" style="aspect-ratio:%d/%d">' % (bw, bh),
            '<div class="monitor__screen" style="%s">%s</div>' % (screen, self.img(
                first['src'], first['alt'], (first['w'], first['h']), 'monitor__shot',
                extra=' data-accent-shot')),
            '<img class="monitor__bezel" src="assets/art/%s" alt="" width="%d" height="%d" '
            'loading="lazy" decoding="async">' % (bezel, bw, bh),
            '</div>',
            '<figcaption class="monitor__caption" data-accent-caption>%s</figcaption>'
            % esc(first['caption']),
            '</figure>',
            '<div class="accents__pick">',
            '<ul class="accents__cards">%s</ul>' % ''.join(cards),
            crt,
            '</div>',
            '</div>'])

    def d_cards(self, body):
        out = ['<ul class="cards">']
        for title, icon, blocks in split_titled(parse(body, '::: cards'), '::: cards', icon=True):
            hid = self.ids.take(title)
            ic = ''
            if icon:
                f = 'pixel-%s.png' % icon
                self.p.assets.size('art/' + f)
                ic = ('<span class="card__icon" aria-hidden="true" style="'
                      '-webkit-mask-image:url(assets/art/%s);mask-image:url(assets/art/%s)">'
                      '</span>' % (f, f))
            out.append('<li class="card">%s<h3 id="%s">%s</h3>%s</li>'
                       % (ic, hid, self.inl(title), self.blocks(blocks)))
        out.append('</ul>')
        return '\n'.join(out)

    def d_steps(self, body):
        out = ['<ol class="steps">']
        for title, _, blocks in split_titled(parse(body, '::: steps'), '::: steps'):
            hid = self.ids.take(title)
            out.append('<li class="step"><h3 id="%s">%s</h3>%s</li>'
                       % (hid, self.inl(title), self.blocks(blocks)))
        out.append('</ol>')
        return '\n'.join(out)

    def d_note(self, body):
        return '<aside class="note">%s</aside>' % self.blocks(parse(body, '::: note'))

    def d_js(self, body):
        blocks = parse(body, '::: js')
        if any(b[0] in ('heading', 'directive') for b in blocks):
            die('::: js: holds text, not a heading or a directive')
        return '<div class="js-only">%s</div>' % self.blocks(blocks)

    def body(self):
        """The page between the header and the footer: what comes before the
        first ## is the hero, and each ## opens a section."""
        out, open_ = [], False
        for b in self.p.walk('html'):
            if b[0] == 'heading' and b[1] == 2:
                if open_:
                    out.append('</div></section>')
                hid = self.ids.take(b[2])
                out.append('<section class="section section--%s" aria-labelledby="%s">'
                           '<div class="wrap"><h2 id="%s">%s</h2>'
                           % (hid, hid, hid, self.inl(b[2])))
                open_ = True
            else:
                out.append(self.block(b))
        if open_:
            out.append('</div></section>')
        return '\n'.join(out)

    def nav(self, base):
        items = []
        for part in self.p.meta['nav'].split(','):
            target, _, label = part.strip().partition(' ')
            if target.startswith('#'):
                if target[1:] not in self.p.ids:
                    die('front matter nav: %s is no heading of the page' % target)
                href = base + target
            else:
                href = self.link(target) if not target.startswith('http') else target
                if base and not href.startswith('http'):
                    href = base + href
            items.append('<li><a href="%s">%s</a></li>' % (esc(href), esc(label.strip())))
        return '\n'.join(items)


# ── the stylesheet's palette, and the script that sets it before paint ───

def mix(a, b, t):
    """Hex colour a moved a fraction t of the way to b."""
    pa = [int(a[i:i + 2], 16) for i in (0, 2, 4)]
    pb = [int(b[i:i + 2], 16) for i in (0, 2, 4)]
    return ''.join('%02x' % int(round(x + (y - x) * t)) for x, y in zip(pa, pb))


def rgba(h, alpha):
    return 'rgb(%d %d %d / %s)' % (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), alpha)


def derived(s):
    """The colours the stylesheet uses beyond libkcolor's nine, each made
    from them: quieter text, rules, the glow, the scanlines, and the tint
    Phosphor mode gives a picture (sepia's hue moved to the accent's)."""
    h = s['hex']
    light = luminance(h['deep']) > 0.5
    r, g, b = (int(h['primary'][i:i + 2], 16) / 255.0 for i in (0, 2, 4))
    hue, _, sat = colorsys.rgb_to_hls(r, g, b)
    return {
        'muted': mix(h['text'], h['deep'], 0.25),
        'line': mix(h['primary'], h['deep'], 0.72),
        'line-soft': mix(h['primary'], h['deep'], 0.86),
        'glow': rgba(h['primary'], '0.35'),
        'scan': rgba('000000', '0.06' if light else '0.3'),
        'crt-hue': '%ddeg' % round(hue * 360 - 39),
        'crt-sat': '%.2f' % (3 * sat if max(r, g, b) - min(r, g, b) > 0.05 else 0),
    }


# The pairs the stylesheet sets text in on a derived colour or in one, each
# held to WCAG AA in every accent: the quieter text on the page, on a card
# and on the alternate sections' ground, and text, quieter text and links on
# the line-soft ground of inline code and of a hovered or open item in the
# bar along the top.
def check_derived(schemes):
    for s in schemes:
        h, d = s['hex'], derived(s)
        pairs = [(d['muted'], 'muted text', h[bg], bg) for bg in ('deep', 'variant', 'backdrop')]
        pairs += [(fg, fname, d['line-soft'], 'line-soft')
                  for fg, fname in ((h['text'], 'text'), (d['muted'], 'muted text'),
                                    (h['primary'], 'primary'))]
        for fg, fname, bg, bname in pairs:
            c = contrast(fg, bg)
            if c < 4.5:
                die('accent %s: %s on %s is %.2f:1, under 4.5:1' % (s['name'], fname, bname, c))


def palette_css(schemes, default):
    out = []
    for s in schemes:
        sel = ':root[data-accent="%s"]' % s['name']
        if s['name'] == default:
            sel = ':root, ' + sel
        light = luminance(s['hex']['deep']) > 0.5
        decls = ['--%s: #%s' % (CSS_VAR[k], s['hex'][k]) for k in KCOL_FIELDS]
        for k, v in derived(s).items():
            decls.append('--%s: %s' % (k, ('#' + v) if re.fullmatch(r'[0-9a-f]{6}', v) else v))
        decls.append('color-scheme: %s' % ('light' if light else 'dark'))
        out.append('%s {\n  %s;\n}' % (sel, ';\n  '.join(decls)))
    return '\n'.join(out)


def head_script(schemes, default, pairs, preload=()):
    """Sets <html data-accent data-phosphor> from the query string or the
    stored choice before the first paint, so a returning reader never sees
    the default flash first. site.js holds the same rules for later
    changes. PRELOAD names the pictures HERO_SCRIPT will give the hero, as
    the path before -<accent>.png: the head script fetches the accent's now,
    since HERO_SCRIPT runs only once the stylesheet has loaded and the
    browser's preload scanner cannot see a file it names."""
    names = [s['name'] for s in schemes]
    return ('(function(d){var A=%s,P=%s,H=%s,a="%s",p="off",q=location.search,m,e;'
            'try{var s=localStorage.getItem("kdos-accent");if(A.indexOf(s)>=0)a=s;'
            'if(localStorage.getItem("kdos-phosphor")==="on")p="on"}catch(x){}'
            'm=q.match(/[?&]accent=([a-z]+)/);if(m&&A.indexOf(m[1])>=0)a=m[1];'
            'm=q.match(/[?&]phosphor=(on|off|1|0)/);if(m)p=(m[1]==="on"||m[1]==="1")?"on":"off";'
            'e=p==="on"&&P.indexOf(a)>=0?"phosphor":a;'
            'd.setAttribute("data-choice",a);d.setAttribute("data-accent",e);'
            'd.setAttribute("data-phosphor",p);d.className+=" js";'
            'H.forEach(function(h){var l=document.createElement("link");l.rel="preload";'
            'l.as="image";l.href=h+"-"+e+".png";l.setAttribute("fetchpriority","high");'
            'document.head.appendChild(l)})})(document.documentElement);'
            % (json.dumps(names), json.dumps(pairs), json.dumps(list(preload)), default))


# A transparent pixel: the hero pictures' src until HERO_SCRIPT names theirs.
EMPTY_GIF = 'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7'

# Run inline right after the hero, before its first paint: the hero's
# pictures, accent name and swatches as the head script's <html data-accent>
# and <html data-choice> say. site.js does the same for the whole page once
# it has loaded, and for every later change. An accent is named in prose
# capitalised, as facts() titles it (data-accent-title), and as a command
# names it where the page shows a terminal (data-accent-name).
HERO_SCRIPT = (
    '(function(d){var r=d.documentElement,a=r.getAttribute("data-accent"),'
    'c=r.getAttribute("data-choice")||a,h=d.querySelector(".hero"),'
    't=function(n){return n.charAt(0).toUpperCase()+n.slice(1)},'
    'f=function(s,g){[].forEach.call(h.querySelectorAll(s),g)};if(!h)return;'
    'f("[data-accent-src]",function(i){i.src=i.getAttribute("data-accent-src").replace("{accent}",a)});'
    'f("[data-accent-name]",function(e){e.textContent=a});'
    'f("[data-accent-title]",function(e){e.textContent=t(a)});'
    'f("[data-accent-via]",function(e){e.textContent=a!==c?"("+t(c)+" + CRT)":""});'
    'f("[data-accent-set]",function(e){e.setAttribute("aria-pressed",'
    'e.getAttribute("data-accent-set")===c?"true":"false")})})(document);')


# ── output ───────────────────────────────────────────────────────────────

def digest(*paths):
    h = hashlib.sha256()
    for p in paths:
        with open(p, 'rb') as f:
            h.update(f.read())
    return h.hexdigest()[:10]


def write(path, text):
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)


def copy_assets(out):
    dst = os.path.join(out, 'assets')
    for sub in ('art', 'fonts', 'shots'):
        src = os.path.join(ASSETS, sub)
        os.makedirs(os.path.join(dst, sub), exist_ok=True)
        for name in sorted(os.listdir(src)):
            if name.endswith('.tsv') or not os.path.isfile(os.path.join(src, name)):
                continue
            shutil.copyfile(os.path.join(src, name), os.path.join(dst, sub, name))
    shutil.copyfile(STYLE, os.path.join(dst, 'site.css'))
    shutil.copyfile(SCRIPT, os.path.join(dst, 'site.js'))
    shutil.copyfile(os.path.join(ASSETS, 'art', 'favicon.ico'), os.path.join(out, 'favicon.ico'))


def render_template(slots, where):
    text = re.sub(r'<!--.*?-->\n?', '', read(TEMPLATE), flags=re.S)
    return substitute(text, slots, where)


STUB_MARK = 'This page of the KDOS book is at'
STUB = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex">
<link rel="canonical" href="{canon}">
<meta http-equiv="refresh" content="0; url={href}">
<script>location.replace({js}+location.hash)</script>
</head>
<body>
<p>{mark} <a href="{href}">{canon}</a>.</p>
</body>
</html>
'''


def book_stubs(out):
    """A stub at the site's root for each book page, sending the reader on
    to book/, fragment and all."""
    book = os.path.join(out, 'book')
    pages = []
    for d, dirs, files in os.walk(book):
        dirs.sort()
        for name in sorted(files):
            if name.endswith('.html'):
                pages.append(os.path.relpath(os.path.join(d, name), book))
    made = 0
    for p in pages:
        if p == 'index.html':
            continue
        stub = os.path.join(out, p)
        if os.path.exists(stub) and STUB_MARK not in read(stub):
            continue
        href = os.path.relpath(os.path.join(book, p), os.path.dirname(stub))
        m = re.search(r'<title>(.*?)</title>', read(os.path.join(book, p)), re.S)
        title = html.unescape(m.group(1).strip()) if m else p
        canon = CANONICAL + 'book/' + p
        write(stub, STUB.format(title=esc(title), canon=esc(canon), href=esc(href),
                                js=json.dumps(href), mark=STUB_MARK))
        made += 1
    return pages, made


KFS_LINK_RE = re.compile(r'href="%skfs/([^"#]*)(?:#([^"]*))?"' % re.escape(CANONICAL))


def check_book_kfs_links(out, book_pages):
    """Every link of the KDOS book into KDOS from Scratch on this site, as
    mkbook.py --site writes it from kfsgen.py's page ids, names a page of
    DIR/kfs and an id that page has: the two books are rendered by
    different scripts, and a link one makes and the other does not is a
    dead link on the site."""
    kfs = os.path.join(out, 'kfs')
    ids = {}
    bad = []
    for p in book_pages:
        text = read(os.path.join(out, 'book', p))
        for m in KFS_LINK_RE.finditer(text):
            page = m.group(1) or 'index.html'
            path = os.path.join(kfs, page)
            if page not in ids:
                ids[page] = set(re.findall(r'\sid="([^"]+)"', read(path))) \
                    if os.path.isfile(path) else None
            if ids[page] is None or (m.group(2) and m.group(2) not in ids[page]):
                bad.append('book/%s: kfs/%s%s' % (p, page, '#' + m.group(2) if m.group(2) else ''))
    if bad:
        die('links from the KDOS book into KDOS from Scratch that land nowhere:\n  '
            + '\n  '.join(bad))


def sitemap(out, book_pages):
    """The homepage and every page of both books, each once: a book's
    index.html is its directory, and the KDOS book's README.html is the
    same page as its index.html."""
    urls = [CANONICAL]
    for p in book_pages:
        if p == 'README.html' and 'index.html' in book_pages:
            continue
        urls.append(CANONICAL + 'book/' + ('' if p == 'index.html' else p))
    kfs = os.path.join(out, 'kfs')
    if os.path.isdir(kfs):
        for d, dirs, files in os.walk(kfs):
            dirs.sort()
            for name in sorted(files):
                if name.endswith('.html'):
                    p = os.path.relpath(os.path.join(d, name), kfs)
                    urls.append(CANONICAL + 'kfs/' + ('' if p == 'index.html' else p))
    seen, body = set(), []
    for u in urls:
        if u not in seen:
            seen.add(u)
            body.append('  <url><loc>%s</loc></url>' % esc(u))
    write(os.path.join(out, 'sitemap.xml'),
          '<?xml version="1.0" encoding="UTF-8"?>\n'
          '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
          + '\n'.join(body) + '\n</urlset>\n')
    return len(body)


# The terminal names the address asked for once site.js has read it from
# location.pathname into each data-404-path; {path} is what it says before.
NOT_FOUND = '''<section class="section section--not-found" aria-labelledby="not-found">
<div class="wrap">
<h1 id="not-found">404: nothing here</h1>
<div class="term term--small"><pre class="term__body" tabindex="0"><span class="term__prompt">kdos@kdos:~ λ</span> cd <span data-404-path>{path}</span>
bash: cd: <span data-404-path>{path}</span>: No such file or directory
<span class="term__prompt">kdos@kdos:~ λ</span> <span class="term__cursor" aria-hidden="true"></span></pre></div>
<p>The page you asked for is not on kdos.dev. The KDOS book is at <a href="/book/">/book/</a>
and <em>KDOS from Scratch</em> at <a href="/kfs/">/kfs/</a>, or start again from
<a href="/">the homepage</a>.</p>
</div>
</section>'''


def render_pages(page, schemes, default, pairs):
    """{name: html} for the homepage and its 404 page."""
    home = Homepage(page)
    body = home.body()
    og = 'social-card.png'
    ow, oh = page.assets.size('art/' + og)

    def slots_for(base, title, canon, nav_base, body_html, robots, preload=()):
        s = dict(page.values)
        s.update({
            'base': base,
            'title': esc(title),
            'description': esc(page.meta['description']),
            'canonical': canon,
            'robots': robots,
            'og_image': CANONICAL + 'assets/art/' + og,
            'og_image_w': str(ow),
            'og_image_h': str(oh),
            'og_image_alt': esc(page.assets.art_alt(og)),
            'palette': palette_css(schemes, default),
            'head_script': head_script(schemes, default, pairs, preload),
            'default_accent': default,
            'theme_color': '#' + [x for x in schemes if x['name'] == default][0]['hex']['deep'],
            'css': base + 'assets/site.css?v=' + digest(STYLE),
            'js': base + 'assets/site.js?v=' + digest(SCRIPT),
            'nav': home.nav(nav_base),
            'body': body_html,
            'crt_pairs': ' '.join(pairs),
        })
        return s

    # The 404 page is served at whatever address was asked for, so its own
    # links and assets are rooted at the site's root, not relative.
    return {
        'index.html': render_template(
            slots_for('', page.meta['title'], CANONICAL, '', body, 'index,follow', home.preload),
            'site/template.html'),
        '404.html': render_template(
            slots_for('/', 'Not found · KDOS', CANONICAL, '/',
                      NOT_FOUND.format(path='/that/page'), 'noindex'),
            'site/template.html'),
    }


def build_site(out, pages):
    for name, text in pages.items():
        write(os.path.join(out, name), text)
    copy_assets(out)
    write(os.path.join(out, 'robots.txt'),
          'User-agent: *\nAllow: /\n\nSitemap: %ssitemap.xml\n' % CANONICAL)
    stubs, book = 0, []
    if os.path.isdir(os.path.join(out, 'book')):
        book, stubs = book_stubs(out)
        if os.path.isdir(os.path.join(out, 'kfs')):
            check_book_kfs_links(out, book)
    return stubs, sitemap(out, book)


OFFSITE = re.compile(r'(https?:)?//')


def check_requests(pages):
    """No page of the homepage loads anything from another host: a script,
    stylesheet, font, picture or frame is a file of the site. Links a reader
    follows, and the canonical and OpenGraph addresses, are not requests."""
    for name, text in pages.items():
        for tag in re.finditer(r'<(script|link|img|iframe|source|video|audio)\b[^>]*>', text):
            t = tag.group(0)
            if t.startswith('<link') and re.search(r'rel="(canonical|alternate)"', t):
                continue
            for m in re.finditer(r'\b(src|href|srcset)="([^"]*)"', t):
                for url in (u.strip().split(' ')[0] for u in m.group(2).split(',')):
                    if OFFSITE.match(url):
                        die('%s: <%s> loads %s from another host' % (name, tag.group(1), url))
        for m in re.finditer(r'url\(\s*[\'"]?([^\'")]*)', text):
            if OFFSITE.match(m.group(1)):
                die('%s: url(%s) is on another host' % (name, m.group(1)))
    css = read(STYLE)
    for m in re.finditer(r'url\(\s*[\'"]?([^\'")]*)', css):
        if OFFSITE.match(m.group(1)):
            die('site/style.css: url(%s) is on another host' % m.group(1))
    if '@import' in css:
        die('site/style.css: @import')
    if re.search(r'\b(fetch|XMLHttpRequest|WebSocket|EventSource|sendBeacon)\b', read(SCRIPT)):
        die('site/site.js: asks the network for something')


def main():
    ap = argparse.ArgumentParser(description="Write README.md and kdos.dev's homepage "
                                             'from site/home.md.')
    ap.add_argument('--out', default=None, help='the homepage directory (default build/site)')
    ap.add_argument('--check', action='store_true',
                    help='write no README.md; exit 1 with a diff when it differs')
    a = ap.parse_args()
    try:
        schemes, default = read_schemes()
        check_art_inputs(schemes, default)
        check_contrast(schemes)
        check_derived(schemes)
        pairs = crt_pairs()
        values = facts(schemes, default)
        assets = Assets()
        page = Page(values, assets, schemes, default, pairs)
        readme = Readme(page).render()
        pages = render_pages(page, schemes, default, pairs)
        check_requests(pages)
        if a.check:
            have = read(README) if os.path.exists(README) else ''
            if have != readme:
                sys.stdout.writelines(difflib.unified_diff(
                    have.splitlines(True), readme.splitlines(True),
                    'README.md', 'README.md (from site/home.md)'))
                print('mksite: README.md is not what site/home.md makes; run `make site`',
                      file=sys.stderr)
                return 1
        else:
            write(README, readme)
        out = a.out if a.out else (None if a.check else os.path.join(ROOT, 'build', 'site'))
        if out:
            stubs, urls = build_site(out, pages)
            print('mksite: %s: the homepage, %d redirect stubs, %d sitemap entries'
                  % (out, stubs, urls))
        if not a.check:
            print('mksite: README.md written')
        else:
            print('mksite: README.md matches site/home.md')
    except SiteError as e:
        print('mksite: ' + str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

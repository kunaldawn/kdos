# Glossary

The vocabulary these documents use, defined once. Where a term has a general meaning elsewhere and
a specific one here, the specific one is what this documentation means.

## Terms

**accent** — One of the four palettes: phosphor, amber, ice, bone. Not a single colour but a small
set of related values that every drawn surface resolves its [slots](#terms) against. Stored as one
word in a cache file, which is the entire theme state the desktop reads. See
[Theming](../02-user-guide/theming.md).

**alien app** — A graphical application that is not compiled by this repository. It ships as a
[pack](#terms) and runs in a [box](#terms). See [Applications](../02-user-guide/applications.md).

**appbox** — The launching and box-management program, `kdos-appbox`. Also, loosely, the mechanism
by which alien apps run. See [kdos-appbox](../04-programs/kdos-appbox.md).

**base pack** — A pack containing a **whole** root filesystem rather than a difference over
another. Two exist. Everything else in the catalogue is a difference over one of them.

**binhost** — A host serving prebuilt host packages, with a signed index. Optional, and one you run
yourself; there is no public archive. See [Packaging](../03-architecture/packaging.md).

**box** — A rootless container in which an alien app runs, composed from a stack of mounted packs
plus a writable layer. One per application. Named after its pack. A box is a packaging and
desktop-interface boundary, **not** a security boundary against you — it shares your home
directory. See [The security model](../03-architecture/security-model.md).

**box profile** — `~/.config/kdos/boxes/<name>.conf`. Every key maps onto a container-engine flag or
onto something KDOS enforces itself, and the profile says which.

**build-config hash** — One of the two hashes that decide whether a prebuilt package is usable: the
architecture, C library, target, compiler version and flags. Written `B:` in an index.

**cell grid** — The model every KDOS surface is drawn in: a two-dimensional buffer of character
cells, each with a character, a foreground slot, a background slot and attributes. See
[the design language](../03-architecture/design-language.md).

**chrome** — The furniture around content: the frame, the header band, group headings, the button
bar, scrollbars. Drawn by one library so there is one implementation of each. Also, **supervised
chrome** — the desktop programs the compositor starts and restarts.

**compose** — To build a box's overlay from its pack stack. Idempotent, reference counted, and
redone before every start because the overlay lives on a temporary filesystem.

**data pack** — A pack carrying a dataset rather than a program. Mounted read-only and
**executable-disabled**, never composed into a box root; what it is *for* is its
[grafts](#terms).

**delta** — A binary difference between two packages or two packs, taken over the **uncompressed**
form. Never trusted: the reconstruction is verified against the hash the signed index already
carries.

**fixture** — A recorded system state that a reader can be pointed at instead of the live machine,
which is what makes readings and decisions testable. See [Testing](../05-developer/testing.md).

**glyph tier** — Which set of drawing characters a surface may use, chosen from the terminal's
capabilities. The middle tier exists because the console font is 512 glyphs and a character it
lacks renders as a **blank**.

**golden** — A committed reference frame: a surface rendered offscreen and compared byte for byte.
Text frames catch geometry; cell frames catch colour as well.

**graft** — A declared placement of a data pack's contents somewhere a consumer will look. Two
namespaces, because the host's data directories are invisible inside a box. Recorded in a manifest
so removal is exact.

**host** — The KDOS system itself: everything compiled from this repository. Contrasted with a
[box](#terms). Also, in a build context, the machine you are building on.

**medium** — The USB stick or image KDOS boots from. On this distribution the medium **is** the
software library, which is why there is no application store.

**pack** — One application, runtime, base or dataset as a single signed file: a filesystem image
with a metadata blob, an icon, a signature block and a footer appended. Extension `.kpack`. See
[Packs and boxes](../03-architecture/packs-and-boxes.md).

**pack lane** — The whole application-packaging path: the catalogue, the bake, the daemon, the
boxes and the generated launchers. Distinguished from the host packaging path, which is a separate
system.

**phase** — One stage of the build, a directory under `script/`. Eight of them, run in sorted
order, each either numbered scripts or a package list.

**port** — One piece of host software as this repository describes it: a directory holding a
`kpkgbuild` and a `build.sh`. There are three port repositories and they use one format. See
[Writing ports](../05-developer/writing-ports.md).

**recipe hash** — The other of the two hashes: a hash over the recipe files, which for a
source-less port covers its own directory and the libraries too. Written `E:` in an index. Decides
what the build rebuilds.

**ring** — Which of the three tiers a piece of software belongs to: core, desktop, or outer. The
rule that decides is the build cost. See [Architecture overview](../03-architecture/overview.md).

**runtime** — A pack shared by many applications, holding a toolkit or a family of libraries. Seven
exist. An application pack is a difference over one.

**session** — A running desktop: the compositor, its supervised chrome, and the per-user services
under it. Started by hand from a terminal.

**shim** — A symlink on your search path named after an application, pointing at the launcher
program, which dispatches on the name it was invoked as. What makes a boxed application an ordinary
command.

**slot** — A named colour role — accent, warning, text, mid, dim, surface, background — that
resolves against the current accent. Everything drawn takes colour from a slot rather than from a
literal value.

**sprite** — A picture occupying whole cells, with its slot and sub-cell position encoded in the
cell itself, so the ordinary row diff is already its damage mechanism. Always optional: every
caller draws a character fallback when none is available.

**tile** — A block of cells drawn as **pixels**, for content that genuinely cannot be a row of text.
Bounded to a modest number of cells. Owns two slots and alternates between them, or its content
would never be presented.

**warmup** — Starting the boxes for your pinned applications in the background at login, so the
first launcher click does not pay for a container start.

**whiteout** — How a filesystem layer records a deletion. The overlay filesystem and a container
image archive use **different** conventions, and a pack built from the wrong one merges with the
deleted file still present.

## Words this documentation avoids

Keeping the vocabulary to one vocabulary:

| Not used | Instead |
|---|---|
| "app store" | The medium **is** the software library; the Start menu is the discovery surface |
| "distro" in prose | Distribution |
| "just" | Say the thing without it. "Just run X" hides how much X is |
| "simply", "obviously", "of course" | If it were obvious the sentence would not be needed |
| "should work" | Say what was measured, or say it is untested |
| First-person decision narration | State the decision and its reason in the present |
| Past-tense narration of any kind — what something was before, what changed, what a fix corrected | This documentation describes the present. See [Principles](../01-philosophy/principles.md#documentation-describes-the-present) |
| "sandbox" for a box, unqualified | A box constrains what an application can do to the **desktop**, not to your data |

## See also

- [Architecture overview](../03-architecture/overview.md) — where most of these terms live
- [Principles](../01-philosophy/principles.md) — the rules behind the vocabulary
- [Command index](command-index.md) — every command by name
- [Configuration](configuration.md) — every setting by name

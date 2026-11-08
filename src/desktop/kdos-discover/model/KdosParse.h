/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — readers for what the KDOS tools print
 *
 * EVERY ANSWER COMES FROM THE TOOL THAT OWNS IT. Whether an application is
 * installed is `kdos-appbox catalogue`'s answer, what a box is made of is
 * `kdos-box list`'s, what the pack store holds is kdos-packd's and what the
 * host has is kpkg's. These functions only read those outputs; a second
 * derivation of any of them here would be a second answer that drifts.
 *
 * The one file read directly is the catalogue itself, for what no command
 * prints: each row's parent chain (how many images an install builds, which is
 * the install's progress) and its Debian packages (which AppStream component
 * describes it).
 * ---------------------------------
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Kdos
{

// ── the catalogue file ──────────────────────────────────────────────────

struct CatalogueRow {
    QString kind; // base, runtime, app or data
    QString id;
    QString parent; // empty for a base
    QStringList packages;
    QString name;
    QString category;
    QString tagline;
    quint64 bytes = 0;
};

struct CatalogueGroup {
    QString id;
    QString description;
    QStringList members;
};

struct Catalogue {
    QHash<QString, CatalogueRow> rows;
    QStringList offered; // app and data rows, in file order
    QList<CatalogueGroup> groups;

    // The rows an install of `id` builds, from the base up; 0 when a parent
    // is missing or the chain runs past 16 rows, where kdos-appbox refuses it.
    int chainLength(const QString &id) const;
    const CatalogueGroup *group(const QString &id) const;
};

Catalogue parseCatalogue(const QByteArray &text);

// ── kdos-appbox catalogue [--groups] ────────────────────────────────────

struct CatalogueListing {
    QString id;
    QString name;
    QString category;
    QString parent;
    QString tagline;
    quint64 bytes = 0;
    bool installed = false;
};

QList<CatalogueListing> parseCatalogueListing(const QByteArray &out);
QList<CatalogueGroup> parseGroupListing(const QByteArray &out);

// ── kdos-packd ──────────────────────────────────────────────────────────

struct PackRow {
    QString id;
    QString version;
    QString kind;
    QString state; // mounted, installed or available
    QString origin; // store or medium
    quint64 size = 0;
};

// A reply is its lines and a last line `ok` or `err <why>`. A reply that
// never reached `ok` is a failure, whatever came before it.
bool replyOk(const QByteArray &reply, QString *error = nullptr);
QList<PackRow> parsePackList(const QByteArray &reply);

// `key = value` lines: packd's `info`, `kdos-pack extract-meta`. A repeated key
// keeps every value, in order.
using Meta = QMap<QString, QStringList>;
Meta parseMeta(const QByteArray &text);
QString metaValue(const Meta &meta, const QString &key);

// ── kdos-box list ───────────────────────────────────────────────────────

struct BoxRow {
    QString name;
    QString base; // "-" when the profile records none
    QString state;
    QString persistence;
    QString accent;
    quint64 disk = 0;
};

// Reads `kdos-box list --json` and, from a kdos-box that does not know the
// flag, the column table it prints instead.
QList<BoxRow> parseBoxList(const QByteArray &out);

// ── kdos-box profile <box> ──────────────────────────────────────────────

// What a box is allowed, as `kdos-box profile` reports it enforced: each
// `key = value  mechanism` line's first value word, and every `!` line, which
// names something asked for that cannot be delivered. `grant` keeps its whole
// list; a box with none says `none`.
struct BoxGrants {
    QMap<QString, QString> values;
    QStringList warnings;
};

BoxGrants parseBoxProfile(const QByteArray &out);

// ── kpkg list --json ────────────────────────────────────────────────────

// name -> version-release
QHash<QString, QString> parseKpkgList(const QByteArray &out);

// ── a .ktar ─────────────────────────────────────────────────────────────

struct Selection {
    QStringList groups;
    QStringList ids;
};

Selection parseSelection(const QByteArray &text);

struct IndexRow {
    QString id; // P:
    QString version; // V:
    QString kind; // K:
    QString name; // N:
    QString summary; // T:
    QString category; // G:
    QString file; // F:
    quint64 size = 0; // S:
};

QList<IndexRow> parsePackIndex(const QByteArray &text);

// ── kdos cve --json ─────────────────────────────────────────────────────

// The findings in `kdos cve --json`; -1 when the output is not its document.
int countCveFindings(const QByteArray &out);

// ── the update tray's rules ─────────────────────────────────────────────

// /var/lib/kdos/update.json offers an update when its `offer` is a version:
// `kdos update check` sets it only when a verified slot image (`lane`
// `image`) or the signed binhost (`lane` `binhost`, covering every package
// that is behind) can take this system there, which is what Discover's
// Updates page installs.
bool updateOffer(const QByteArray &updateJson, QString *version = nullptr);

struct SlotsState;

// The same offer with kdos-sysupdated's CheckNew rules (state_current_offer)
// applied against the boot state, for the tray when kdos-sysupdated does not
// answer: nothing while this boot is a trial (the update would go into the
// confirmed way back), nothing when the document is not this root's (its
// `installed` is not the running slot's version), and nothing when the armed
// slot already holds exactly the offered version.
bool updateOffer(const QByteArray &updateJson, const SlotsState &boot, QString *version = nullptr);

// The findings of `kdos cve --json` the offered update closes: a finding
// whose port is one of update.json's `packages`, which the update takes to a
// `want` version at or past the finding's `fixed_in`. A finding the update
// leaves open is a known hole, not a security update. -1 when either
// document is not what its tool writes.
int securityFixes(const QByteArray &updateJson, const QByteArray &cveJson);

// What the tray reads from io.kdos.Slots1's State, or from `kdos-bootctl
// status --json` when kdos-sysupdated does not answer.
struct SlotsState {
    // State carries `offer`: what CheckNew answers, worked out from the
    // update document already on disk, so reading it fetches nothing.
    // bootctl's document has no such key and leaves this false.
    bool offerKnown = false;
    QString offer;
    // A slot armed by `kdos-bootctl try` that is not the one running.
    bool rebootRequired = false;
    // "", "acquire" or "install", and its items.
    QString job;
    quint64 jobDone = 0;
    quint64 jobTotal = 0;
    // The boot state: the slot running, the slot armed or on trial ("" for
    // none), and the version each holds ("" when not recorded).
    QString running;
    QString trying;
    QString runningVersion;
    QString otherVersion;
};

// State's own keys win (`rebootRequired`, `jobDone`, `jobTotal`); a document
// without them is bootctl's, read by `trying` against `running`.
SlotsState readSlotsState(const QVariantMap &state);

// A restart is pending: readSlotsState(state).rebootRequired.
bool slotArmed(const QVariantMap &state);

// ── small rules ─────────────────────────────────────────────────────────

// An id a KDOS tool accepts as an argument: a catalogue id, a group, a pack
// id, a box name. 1 to 63 bytes of [A-Za-z0-9._-], not starting with `.` or
// `-`, so a name read from a file can never be taken for an option.
bool isSafeId(const QString &id);

// A Debian package name: [a-z0-9][a-z0-9.+-]*, at most 63 bytes.
bool isDebianPackage(const QString &name);

// The box a pack is composed into: `box.<name>`, what kdos-box freeze writes,
// goes back to <name>; every other pack's box is named after the pack.
QString boxNameForPack(const QString &packId);

// The box the store lane builds for a Debian package outside the catalogue:
// `deb.<package>`, with each `+` spelled `_`.
QString boxNameForDebian(const QString &package);

// kpkg's version comparison (libkpkg's kp_vercmp): `1.10` is newer than `1.9`.
int compareVersions(const QString &a, const QString &b);

// What kb_human_size printed, back to bytes: "12M" -> 12582912, "-" -> 0.
quint64 parseHumanSize(const QString &text);

// `A;B;` shows as A, as every catalogue surface shows it.
QString firstCategory(const QString &categories);

}

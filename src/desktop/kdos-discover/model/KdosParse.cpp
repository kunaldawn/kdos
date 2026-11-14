/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — readers for what the KDOS tools print
 * ---------------------------------
 */

#include "KdosParse.h"
#include "KdosEntry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

extern "C" int kp_vercmp(const char *a, const char *b);

namespace Kdos
{

QString originOf(Lane lane, const QString &packOrigin)
{
    switch (lane) {
    case Lane::Catalogue:
        if (packOrigin == "medium"_L1)
            return u"medium"_s;
        return packOrigin == "store"_L1 ? u"kdos-packs"_s : u"kdos-store"_s;
    case Lane::Set:
        return u"kdos-packs"_s;
    case Lane::Pack:
        return packOrigin == "medium"_L1 ? u"medium"_s : u"kdos-packs"_s;
    case Lane::Debian:
        return u"kdos-store"_s;
    case Lane::Box:
        return u"kdos-boxes"_s;
    case Lane::Port:
        return u"kdos-ports"_s;
    case Lane::PortSource:
        return u"kdos-ports-source"_s;
    case Lane::Archive:
        return packOrigin == "medium"_L1 ? u"medium"_s : u"local"_s;
    case Lane::PackFile:
        return u"local"_s;
    }
    return {};
}

static QList<QByteArray> lines(const QByteArray &text)
{
    QList<QByteArray> out = text.split('\n');
    for (QByteArray &l : out) {
        if (l.endsWith('\r'))
            l.chop(1);
    }
    return out;
}

static QStringList words(const QString &line)
{
    static const QRegularExpression space(u"\\s+"_s);
    return line.split(space, Qt::SkipEmptyParts);
}

// ── the catalogue file ──────────────────────────────────────────────────

int Catalogue::chainLength(const QString &id) const
{
    int n = 0;
    QString at = id;
    while (!at.isEmpty()) {
        const auto it = rows.constFind(at);
        if (it == rows.constEnd() || ++n > 16)
            return 0;
        at = it->parent;
    }
    return n;
}

const CatalogueGroup *Catalogue::group(const QString &id) const
{
    for (const CatalogueGroup &g : groups) {
        if (g.id == id)
            return &g;
    }
    return nullptr;
}

Catalogue parseCatalogue(const QByteArray &text)
{
    static const QStringList packKinds{u"base"_s, u"runtime"_s, u"app"_s, u"data"_s};
    Catalogue cat;

    for (const QByteArray &raw : lines(text)) {
        const QString line = QString::fromUtf8(raw).trimmed();
        // A `#` starts a comment only as the first character of a line, so a
        // tagline may carry one.
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList w = words(line);
        if (w.size() < 2)
            continue;

        if (packKinds.contains(w[0]) && w.size() >= 3) {
            CatalogueRow row;
            row.kind = w[0];
            row.id = w[1];
            row.parent = w[2] == "-"_L1 ? QString() : w[2];
            for (int i = 3; i < w.size(); i++) {
                if (w[i] != "-"_L1)
                    row.packages << w[i];
            }
            if (!cat.rows.contains(row.id) && (row.kind == "app"_L1 || row.kind == "data"_L1))
                cat.offered << row.id;
            cat.rows.insert(row.id, row);
        } else if (w[0] == "meta"_L1 && w.size() >= 3) {
            // Applied as it is read: a meta line above its row describes
            // nothing, exactly as kdos-appbox drops it.
            auto it = cat.rows.find(w[1]);
            if (it == cat.rows.end())
                continue;
            const QString rest = line.section(QRegularExpression(u"\\s+"_s), 2);
            const QStringList f = rest.split(u'|');
            it->name = f.value(0).trimmed();
            it->category = firstCategory(f.value(1));
            it->bytes = f.value(2).trimmed().toULongLong();
            it->tagline = f.mid(3).join(u'|').trimmed();
        } else if (w[0] == "group"_L1 && w.size() >= 3) {
            // kdos-appbox's rule: a row naming an app. or data. id lists
            // members, and the first row that names none is the description.
            CatalogueGroup *g = nullptr;
            for (CatalogueGroup &have : cat.groups) {
                if (have.id == w[1])
                    g = &have;
            }
            if (!g) {
                cat.groups.append(CatalogueGroup{w[1], {}, {}});
                g = &cat.groups.last();
            }
            bool any = false;
            for (int i = 2; i < w.size(); i++) {
                if (w[i].startsWith("app."_L1) || w[i].startsWith("data."_L1)) {
                    g->members << w[i];
                    any = true;
                }
            }
            if (!any && g->description.isEmpty())
                g->description = line.section(QRegularExpression(u"\\s+"_s), 2);
        }
    }
    return cat;
}

// ── kdos-appbox catalogue [--groups] ────────────────────────────────────

QList<CatalogueListing> parseCatalogueListing(const QByteArray &out)
{
    QList<CatalogueListing> list;
    for (const QByteArray &raw : lines(out)) {
        const QStringList f = QString::fromUtf8(raw).split(u'\t');
        if (f.size() < 6 || !isSafeId(f[0]))
            continue;
        CatalogueListing l;
        l.id = f[0];
        l.name = f[1];
        l.category = firstCategory(f[2]);
        l.bytes = f[3].toULongLong();
        l.parent = f[4] == "-"_L1 ? QString() : f[4];
        l.installed = f[5] == "installed"_L1;
        l.tagline = f.mid(6).join(u'\t');
        list << l;
    }
    return list;
}

QHash<QString, ChainRow> parseChainListing(const QByteArray &out)
{
    QHash<QString, ChainRow> chain;
    for (const QByteArray &raw : lines(out)) {
        const QStringList f = QString::fromUtf8(raw).split(u'\t');
        if (f.size() < 4 || !isSafeId(f[0]))
            continue;
        ChainRow r;
        r.id = f[0];
        r.bytes = f[1].toULongLong();
        r.parent = f[2] == "-"_L1 ? QString() : f[2];
        r.built = f[3] == "built"_L1;
        chain.insert(r.id, r);
    }
    return chain;
}

quint64 unbuiltChainBytes(const QHash<QString, ChainRow> &chain, const QString &parent)
{
    quint64 sum = 0;
    QString at = parent;
    for (int depth = 0; !at.isEmpty() && depth < 16; depth++) {
        const auto it = chain.constFind(at);
        if (it == chain.constEnd())
            break;
        if (!it->built)
            sum += it->bytes;
        at = it->parent;
    }
    return sum;
}

QList<CatalogueGroup> parseGroupListing(const QByteArray &out)
{
    QList<CatalogueGroup> list;
    for (const QByteArray &raw : lines(out)) {
        const QStringList f = QString::fromUtf8(raw).split(u'\t');
        if (f.size() < 2 || !isSafeId(f[0]))
            continue;
        CatalogueGroup g{f[0], f[1], {}};
        for (const QString &m : words(f.value(2))) {
            if (isSafeId(m))
                g.members << m;
        }
        list << g;
    }
    return list;
}

// ── kdos-packd ──────────────────────────────────────────────────────────

bool replyOk(const QByteArray &reply, QString *error)
{
    const QList<QByteArray> l = lines(reply.trimmed());
    const QString last = l.isEmpty() ? QString() : QString::fromUtf8(l.last());
    if (last == "ok"_L1 || last.startsWith("ok "_L1))
        return true;
    if (error) {
        *error = last.startsWith("err "_L1) ? last.mid(4) : last;
        if (error->isEmpty())
            *error = u"no answer"_s;
    }
    return false;
}

QList<PackRow> parsePackList(const QByteArray &reply)
{
    QList<PackRow> list;
    if (!replyOk(reply))
        return list;
    for (const QByteArray &raw : lines(reply)) {
        const QStringList f = QString::fromUtf8(raw).split(u'\t');
        if (f.size() < 6 || !isSafeId(f[0]))
            continue;
        list << PackRow{f[0], f[1], f[2], f[3], f[5], f[4].toULongLong()};
    }
    return list;
}

Meta parseMeta(const QByteArray &text)
{
    Meta meta;
    for (const QByteArray &raw : lines(text)) {
        const QString line = QString::fromUtf8(raw);
        const qsizetype eq = line.indexOf(u'=');
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (key.isEmpty() || key.contains(u' '))
            continue;
        meta[key] << line.mid(eq + 1).trimmed();
    }
    return meta;
}

QString metaValue(const Meta &meta, const QString &key)
{
    const QStringList v = meta.value(key);
    return v.isEmpty() ? QString() : v.first();
}

// ── kdos-box list ───────────────────────────────────────────────────────

QList<BoxRow> parseBoxList(const QByteArray &out)
{
    QList<BoxRow> list;
    const QByteArray trimmed = out.trimmed();

    // `--json` prints an array of box objects; an object holding that array
    // under `boxes` is read the same, as the Boxes settings page reads it.
    if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed);
        const QJsonArray boxes = doc.isArray() ? doc.array() : doc.object().value("boxes"_L1).toArray();
        for (const QJsonValue &v : boxes) {
            const QJsonObject o = v.toObject();
            BoxRow b;
            b.name = o.value("name"_L1).toString();
            if (!isSafeId(b.name))
                continue;
            // The document says "" for a box with no recorded base, the
            // table "-"; a BoxRow always says "-".
            b.base = o.value("base"_L1).toString();
            if (b.base.isEmpty())
                b.base = u"-"_s;
            b.state = o.value("state"_L1).toString();
            b.persistence = o.value("persistence"_L1).toString();
            b.accent = o.value("accent"_L1).toString();
            // Bytes as a number, or the column's human size as a string.
            const QJsonValue disk = o.value("disk"_L1);
            b.disk = disk.isString() ? parseHumanSize(disk.toString()) : quint64(disk.toDouble());
            list << b;
        }
        return list;
    }

    // BOX BASE STATE PERSIST DISK ACCENT, padded with spaces. No name or base
    // holds a space and the last three columns are single words, so whatever
    // lies between the base and the persistence is the state: "not created"
    // is two words.
    for (const QByteArray &raw : lines(out)) {
        const QStringList w = words(QString::fromUtf8(raw));
        if (w.size() < 6 || w[0] == "BOX"_L1 || !isSafeId(w[0]))
            continue;
        BoxRow b;
        b.name = w[0];
        b.base = w[1];
        b.state = w.mid(2, w.size() - 5).join(u' ');
        b.persistence = w[w.size() - 3];
        b.disk = parseHumanSize(w[w.size() - 2]);
        b.accent = w[w.size() - 1];
        list << b;
    }
    return list;
}

// ── kdos-box profile <box> ──────────────────────────────────────────────

BoxGrants parseBoxProfile(const QByteArray &out)
{
    BoxGrants g;
    for (const QByteArray &raw : lines(out)) {
        const QString line = QString::fromUtf8(raw);
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("! "_L1)) {
            g.warnings << trimmed.mid(2).trimmed();
            continue;
        }
        // A grant's own explanation lines are indented and have no `=`.
        if (line.startsWith(u' ') || line.startsWith(u'\t'))
            continue;
        const qsizetype eq = line.indexOf(" = "_L1);
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        if (key.isEmpty() || key.contains(u' '))
            continue;
        const QString rest = line.mid(eq + 3).trimmed();
        if (key == "grant"_L1)
            g.values.insert(key, rest.startsWith("none"_L1) ? u"none"_s : rest);
        else
            g.values.insert(key, rest.section(u' ', 0, 0, QString::SectionSkipEmpty));
    }
    return g;
}

// ── kpkg list --json ────────────────────────────────────────────────────

QHash<QString, QString> parseKpkgList(const QByteArray &out)
{
    QHash<QString, QString> installed;
    const QJsonArray packages = QJsonDocument::fromJson(out).object().value("packages"_L1).toArray();
    for (const QJsonValue &v : packages) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name"_L1).toString();
        if (name.isEmpty())
            continue;
        const QString release = o.value("release"_L1).toString();
        const QString version = o.value("version"_L1).toString();
        installed.insert(name, release.isEmpty() ? version : version + u'-' + release);
    }
    return installed;
}

// ── a .ktar ─────────────────────────────────────────────────────────────

Selection parseSelection(const QByteArray &text)
{
    Selection s;
    for (const QByteArray &raw : lines(text)) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList w = words(line);
        if (w.size() == 2 && w[0] == "group"_L1) {
            if (isSafeId(w[1]))
                s.groups << w[1];
        } else if (w.size() == 1 && isSafeId(w[0]) && !s.ids.contains(w[0])) {
            // Import reads only the bare id lines; a group line records what
            // was picked and installs nothing of its own.
            s.ids << w[0];
        }
    }
    return s;
}

QList<IndexRow> parsePackIndex(const QByteArray &text)
{
    QList<IndexRow> rows;
    IndexRow row;
    auto flush = [&]() {
        if (isSafeId(row.id))
            rows << row;
        row = IndexRow{};
    };
    for (const QByteArray &raw : lines(text)) {
        if (raw.trimmed().isEmpty()) {
            flush();
            continue;
        }
        if (raw.size() < 2 || raw[1] != ':')
            continue;
        const QString value = QString::fromUtf8(raw.mid(2)).trimmed();
        switch (raw[0]) {
        case 'P':
            row.id = value;
            break;
        case 'V':
            row.version = value;
            break;
        case 'K':
            row.kind = value;
            break;
        case 'N':
            row.name = value;
            break;
        case 'T':
            row.summary = value;
            break;
        case 'G':
            row.category = value;
            break;
        case 'F':
            row.file = value;
            break;
        case 'S':
            row.size = value.toULongLong();
            break;
        default:
            break;
        }
    }
    flush();
    return rows;
}

// ── kdos cve --json ─────────────────────────────────────────────────────

int countCveFindings(const QByteArray &out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isObject())
        return -1;
    return int(doc.object().value("findings"_L1).toArray().size());
}

// ── kdos update install --dry-run ───────────────────────────────────────

InstallPreview parseInstallPreview(const QByteArray &out)
{
    InstallPreview p;
    const QList<QByteArray> lines = out.split('\n');
    for (const QByteArray &raw : lines) {
        const QList<QByteArray> w = raw.simplified().split(' ');
        if (w.size() == 4 && w[0] == "total") {
            bool a = false, b = false, c = false;
            const int downloads = w[1].toInt(&a);
            const quint64 bytes = w[2].toULongLong(&b);
            const int builds = w[3].toInt(&c);
            if (!a || !b || !c)
                continue;
            p.valid = true;
            p.downloads += downloads;
            p.bytes += bytes;
            p.builds += builds;
        } else if (w.size() == 3 && (w[1] == "source" || w[1] == "dev-source")) {
            const QString name = QString::fromUtf8(w[0]);
            if (!p.built.contains(name))
                p.built << name;
        }
    }
    return p;
}

// ── /etc/kdos/update.conf ───────────────────────────────────────────────

bool installsFromSource(const QByteArray &updateConf)
{
    const QList<QByteArray> lines = updateConf.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray l = raw.trimmed();
        if (!l.startsWith("install") || l.startsWith('#'))
            continue;
        const QByteArray rest = l.mid(7);
        if (!rest.isEmpty() && rest[0] != ' ' && rest[0] != '\t' && rest[0] != '=')
            continue;
        qsizetype i = 0;
        while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t' || rest[i] == '='))
            i++;
        const QByteArray value = rest.mid(i).split(' ').value(0).split('\t').value(0).trimmed();
        if (value.isEmpty())
            continue;
        return value == "source";
    }
    return false;
}

// ── the update tray's two rules ─────────────────────────────────────────

bool updateOffer(const QByteArray &updateJson, QString *version)
{
    const QJsonValue offer = QJsonDocument::fromJson(updateJson).object().value("offer"_L1);
    if (!offer.isString() || offer.toString().isEmpty())
        return false;
    if (version)
        *version = offer.toString();
    return true;
}

bool updateOffer(const QByteArray &updateJson, const SlotsState &boot, QString *version)
{
    QString offer;
    if (!updateOffer(updateJson, &offer))
        return false;
    if (!boot.trying.isEmpty() && boot.trying == boot.running)
        return false;
    const QString installed = QJsonDocument::fromJson(updateJson).object().value("installed"_L1).toString();
    if (!boot.runningVersion.isEmpty() && installed != boot.runningVersion)
        return false;
    if (!boot.trying.isEmpty() && boot.trying != boot.running && boot.otherVersion == offer)
        return false;
    if (version)
        *version = offer;
    return true;
}

// A version without kpkg's `-<release>` or Alpine's `-r<n>`, the form
// kp_vercmp compares a `want` and a `fixed_in` in.
static QString bareVersion(const QString &v)
{
    static const QRegularExpression release(u"-r?[0-9]+$"_s);
    QString out = v.trimmed();
    out.remove(release);
    return out;
}

int securityFixes(const QByteArray &updateJson, const QByteArray &cveJson)
{
    const QJsonDocument update = QJsonDocument::fromJson(updateJson);
    const QJsonDocument cve = QJsonDocument::fromJson(cveJson);
    if (!update.isObject() || !cve.isObject())
        return -1;

    QHash<QString, QString> wanted;
    const QJsonArray packages = update.object().value("packages"_L1).toArray();
    for (const QJsonValue &v : packages) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name"_L1).toString();
        if (!name.isEmpty())
            wanted.insert(name, bareVersion(o.value("want"_L1).toString()));
    }

    int fixed = 0;
    const QJsonArray findings = cve.object().value("findings"_L1).toArray();
    for (const QJsonValue &v : findings) {
        const QJsonObject o = v.toObject();
        const auto want = wanted.constFind(o.value("port"_L1).toString());
        if (want == wanted.constEnd())
            continue;
        const QString fixedIn = bareVersion(o.value("fixed_in"_L1).toString());
        // No recorded fix version: the update moves the port, which is the
        // most the data can say.
        if (fixedIn.isEmpty() || compareVersions(*want, fixedIn) >= 0)
            fixed++;
    }
    return fixed;
}

SlotsState readSlotsState(const QVariantMap &state)
{
    SlotsState s;
    if (state.contains(u"offer"_s)) {
        s.offerKnown = true;
        s.offer = state.value(u"offer"_s).toString();
    }
    if (state.contains(u"rebootRequired"_s)) {
        s.rebootRequired = state.value(u"rebootRequired"_s).toBool();
    } else {
        const QString trying = state.value(u"trying"_s).toString();
        const QString running = state.value(u"running"_s).toString();
        s.rebootRequired = !trying.isEmpty() && trying != running;
    }
    s.running = state.value(u"running"_s).toString();
    s.trying = state.value(u"trying"_s).toString();
    // bootctl's `slots`, {"a": {"version": …}, "b": {…}}.
    const QVariantMap slots = state.value(u"slots"_s).toMap();
    if (!s.running.isEmpty()) {
        const QString other = s.running == "a"_L1 ? u"b"_s : u"a"_s;
        s.runningVersion = slots.value(s.running).toMap().value(u"version"_s).toString();
        s.otherVersion = slots.value(other).toMap().value(u"version"_s).toString();
    }
    s.job = state.value(u"job"_s).toString();
    s.jobDone = state.value(state.contains(u"jobDone"_s) ? u"jobDone"_s : u"job_done"_s).toULongLong();
    s.jobTotal = state.value(state.contains(u"jobTotal"_s) ? u"jobTotal"_s : u"job_total"_s).toULongLong();
    return s;
}

bool slotArmed(const QVariantMap &state)
{
    return readSlotsState(state).rebootRequired;
}

// ── small rules ─────────────────────────────────────────────────────────

bool isSafeId(const QString &id)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_][A-Za-z0-9._-]{0,62}$"_s);
    return re.match(id).hasMatch();
}

bool isDebianPackage(const QString &name)
{
    static const QRegularExpression re(u"^[a-z0-9][a-z0-9.+-]{0,62}$"_s);
    return re.match(name).hasMatch();
}

QString boxNameForPack(const QString &packId)
{
    if (packId.startsWith("box."_L1) && isSafeId(packId.mid(4)))
        return packId.mid(4);
    return packId;
}

QString boxNameForDebian(const QString &package)
{
    // A box name has no `+` and a Debian package name has no `_`, so each `+`
    // becomes `_` and no two packages share a box. kdos-appbox names the box
    // of `install deb:<package>` by the same rule.
    QString name = package;
    name.replace(u'+', u'_');
    return u"deb."_s + name;
}

int compareVersions(const QString &a, const QString &b)
{
    return kp_vercmp(a.toUtf8().constData(), b.toUtf8().constData());
}

quint64 parseHumanSize(const QString &text)
{
    static const QRegularExpression re(u"^([0-9]+(?:\\.[0-9]+)?)([BKMGTP]?)$"_s);
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch())
        return 0;
    static const QString units = u"BKMGTP"_s;
    double v = m.captured(1).toDouble();
    const qsizetype power = m.captured(2).isEmpty() ? 0 : units.indexOf(m.captured(2));
    for (qsizetype i = 0; i < power; i++)
        v *= 1024.0;
    return quint64(v);
}

QString firstCategory(const QString &categories)
{
    const QString first = categories.section(u';', 0, 0).trimmed();
    return first.isEmpty() ? u"Other"_s : first;
}

}

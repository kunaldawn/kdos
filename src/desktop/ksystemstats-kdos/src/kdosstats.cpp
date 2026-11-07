/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   ksystemstats-kdos — what the sensors are read from
 */

#include "kdosstats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include <sys/stat.h>

namespace Kdos
{

namespace
{
const QLatin1String ScopePrefix("app-kdos-");
const QLatin1String ScopeSuffix(".scope");

/* sysfs files are small, and a file that grew past this is not one of them. */
constexpr qint64 MaxSysfsRead = 64 * 1024;

QByteArray readSmall(const QString &path, bool *ok = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    if (ok) {
        *ok = true;
    }
    return f.read(MaxSysfsRead);
}

qint64 readNumber(const QString &path)
{
    bool ok = false;
    const QByteArray b = readSmall(path, &ok).trimmed();
    if (!ok) {
        return -1;
    }
    if (b == "max") {
        return 0;
    }
    bool num = false;
    const qint64 v = b.toLongLong(&num);
    return num && v >= 0 ? v : -1;
}

/* systemd's escaping: `\xHH` for each byte that may not stand in a unit name.
 * Anything that is not a well-formed escape is taken literally. */
QString unescapeUnit(const QString &s)
{
    QByteArray out;
    const QByteArray in = s.toUtf8();
    for (qsizetype i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 3 < in.size() && in[i + 1] == 'x') {
            bool ok = false;
            const int v = in.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                out.append(char(v));
                i += 3;
                continue;
            }
        }
        out.append(in[i]);
    }
    return QString::fromUtf8(out);
}

void collectScopes(const QString &dir, uid_t uid, QList<BoxScope> &out)
{
    const QDir d(dir);
    const QStringList names = d.entryList({ScopePrefix + QLatin1String("*") + ScopeSuffix}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &name : names) {
        const QString box = kdosBoxFromScope(name);
        if (box.isEmpty()) {
            continue;
        }
        const QString path = d.filePath(name);
        struct stat st;
        if (::lstat(QFile::encodeName(path).constData(), &st) != 0 || st.st_uid != uid) {
            continue;
        }
        out.append({box, path});
    }
}

/* A desktop file's [Desktop Entry] group as key → value, with no localised
 * keys: the image is English-only, and a launcher kdos-appbox writes has none. */
QHash<QString, QString> desktopEntry(const QString &path)
{
    QHash<QString, QString> keys;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return keys;
    }
    bool inEntry = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine(4096)).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('['))) {
            if (inEntry) {
                break;
            }
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry) {
            continue;
        }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        if (!keys.contains(key)) {
            keys.insert(key, line.mid(eq + 1).trimmed());
        }
    }
    return keys;
}

QString slotLetter(const QJsonValue &v)
{
    return v.isString() ? v.toString().toUpper() : QString();
}
} // namespace

QString kdosBoxFromScope(const QString &scopeName)
{
    if (!scopeName.startsWith(ScopePrefix) || !scopeName.endsWith(ScopeSuffix)) {
        return {};
    }
    const QString middle = scopeName.mid(ScopePrefix.size(), scopeName.size() - ScopePrefix.size() - ScopeSuffix.size());
    const qsizetype dash = middle.lastIndexOf(QLatin1Char('-'));
    if (dash <= 0 || dash == middle.size() - 1) {
        return {};
    }
    const QString n = middle.mid(dash + 1);
    if (!std::all_of(n.cbegin(), n.cend(), [](QChar c) {
            return c.isDigit();
        })) {
        return {};
    }
    const QString box = unescapeUnit(middle.left(dash));
    if (box.isEmpty() || box.contains(QLatin1Char('/'))) {
        return {};
    }
    return box;
}

QList<BoxScope> kdosFindBoxScopes(const QString &root, uid_t uid)
{
    QList<BoxScope> out;
    const QDir r(root);
    const QStringList tops = r.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &top : tops) {
        const QString delegated = r.filePath(top) + QLatin1String("/delegated");
        if (QFileInfo(delegated).isDir()) {
            collectScopes(delegated, uid, out);
        }
    }
    const QString u = QString::number(uid);
    collectScopes(r.filePath(QLatin1String("user.slice/user-%1.slice/user@%1.service/app.slice").arg(u)), uid, out);
    std::sort(out.begin(), out.end(), [](const BoxScope &a, const BoxScope &b) {
        return a.box != b.box ? a.box < b.box : a.path < b.path;
    });
    return out;
}

CgroupCounters kdosReadCgroup(const QString &dir)
{
    CgroupCounters c;
    if (!QFileInfo(dir).isDir()) {
        return c;
    }
    c.present = true;

    bool ok = false;
    const QByteArray cpu = readSmall(dir + QLatin1String("/cpu.stat"), &ok);
    if (ok) {
        for (const QByteArray &line : cpu.split('\n')) {
            if (line.startsWith("usage_usec ")) {
                bool num = false;
                const qint64 v = line.mid(11).trimmed().toLongLong(&num);
                if (num && v >= 0) {
                    c.cpuUsec = v;
                }
            }
        }
    }

    c.memoryBytes = readNumber(dir + QLatin1String("/memory.current"));
    c.memoryMaxBytes = readNumber(dir + QLatin1String("/memory.max"));
    c.processes = readNumber(dir + QLatin1String("/pids.current"));

    const QByteArray io = readSmall(dir + QLatin1String("/io.stat"), &ok);
    if (ok) {
        c.readBytes = 0;
        c.writeBytes = 0;
        for (const QByteArray &line : io.split('\n')) {
            for (const QByteArray &field : line.split(' ')) {
                bool num = false;
                if (field.startsWith("rbytes=")) {
                    const qint64 v = field.mid(7).toLongLong(&num);
                    if (num && v > 0) {
                        c.readBytes += v;
                    }
                } else if (field.startsWith("wbytes=")) {
                    const qint64 v = field.mid(7).toLongLong(&num);
                    if (num && v > 0) {
                        c.writeBytes += v;
                    }
                }
            }
        }
    }
    return c;
}

CgroupCounters kdosAddCounters(const CgroupCounters &a, const CgroupCounters &b)
{
    auto add = [](qint64 x, qint64 y) -> qint64 {
        if (x < 0) {
            return y;
        }
        if (y < 0) {
            return x;
        }
        return x + y;
    };
    CgroupCounters c;
    c.present = a.present || b.present;
    c.cpuUsec = add(a.cpuUsec, b.cpuUsec);
    c.memoryBytes = add(a.memoryBytes, b.memoryBytes);
    /* A limit of "max" (0) on either scope leaves the box unlimited. */
    c.memoryMaxBytes = (a.memoryMaxBytes == 0 || b.memoryMaxBytes == 0) ? 0 : add(a.memoryMaxBytes, b.memoryMaxBytes);
    c.readBytes = add(a.readBytes, b.readBytes);
    c.writeBytes = add(a.writeBytes, b.writeBytes);
    c.processes = add(a.processes, b.processes);
    return c;
}

QHash<QString, BoxLauncher> kdosBoxLaunchers(const QStringList &dirs)
{
    QHash<QString, BoxLauncher> out;
    QHash<QString, bool> exact;
    for (const QString &dir : dirs) {
        const QDir d(dir);
        QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files | QDir::Readable);
        files.sort();
        for (const QString &file : files) {
            const auto keys = desktopEntry(d.filePath(file));
            const QString box = keys.value(QStringLiteral("X-KDOS-Box"));
            if (box.isEmpty()) {
                continue;
            }
            /* The launcher named exactly for the box is the box's own; a
             * multi-launcher pack's others name it only when it has none. */
            const bool isExact = file == box + QLatin1String(".desktop");
            if (out.contains(box) && (exact.value(box) || !isExact)) {
                continue;
            }
            out.insert(box, {keys.value(QStringLiteral("Name")), keys.value(QStringLiteral("Icon"))});
            exact.insert(box, isExact);
        }
    }
    return out;
}

EnergyReport kdosParseEnergyReport(const QByteArray &reply)
{
    EnergyReport r;
    const QByteArray t = reply.trimmed();
    if (t.startsWith("err ")) {
        r.error = QString::fromUtf8(t.mid(4));
        return r;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(t, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("not a report");
        return r;
    }
    const QJsonObject o = doc.object();
    r.ok = true;
    r.windowSeconds = o.value(QLatin1String("window_s")).toDouble();
    r.samples = o.value(QLatin1String("samples")).toInt();
    r.domains = o.value(QLatin1String("domains")).toString();
    r.idleFloorWatts = o.value(QLatin1String("idle_floor_w")).toDouble();
    r.floorSettled = o.value(QLatin1String("floor_settled")).toBool();
    r.attributable = o.value(QLatin1String("attributable_fraction")).toDouble();
    r.shortLived = o.value(QLatin1String("short_lived")).toDouble();
    r.suspendedWindows = o.value(QLatin1String("suspended_windows")).toInt();

    QHash<QString, int> seen;
    const QJsonArray apps = o.value(QLatin1String("apps")).toArray();
    for (const QJsonValue &v : apps) {
        const QJsonObject a = v.toObject();
        EnergyApp e;
        e.name = a.value(QLatin1String("name")).toString();
        e.app = a.value(QLatin1String("app")).toString();
        if (e.app.isEmpty()) {
            e.app = e.name;
        }
        e.box = a.value(QLatin1String("box")).toString();
        e.impact = a.value(QLatin1String("impact")).toDouble();
        e.cpuSeconds = a.value(QLatin1String("cpu_s")).toDouble();
        e.gpuShare = a.contains(QLatin1String("gpu_share")) ? a.value(QLatin1String("gpu_share")).toDouble() : -1;
        if (e.app.isEmpty()) {
            continue;
        }
        /* Two rows with one id would be one sensor object: the second is
         * numbered rather than dropped. */
        QString id = kdosEnergyObjectId(e.app, e.box);
        const int n = seen.value(id);
        seen.insert(id, n + 1);
        if (n) {
            id += QLatin1Char('~') + QString::number(n + 1);
        }
        e.id = id;
        r.apps.append(e);
    }
    return r;
}

QString kdosEnergyObjectId(const QString &app, const QString &box)
{
    QString id = box.isEmpty() ? app : app + QLatin1Char('@') + box;
    for (QChar &c : id) {
        const bool keep = (c.unicode() < 128 && c.isLetterOrNumber()) || c == QLatin1Char('.') || c == QLatin1Char('_') || c == QLatin1Char('-')
            || c == QLatin1Char('@');
        if (!keep) {
            c = QLatin1Char('_');
        }
    }
    return id;
}

SlotState kdosParseSlotState(const QByteArray &json)
{
    SlotState s;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return s;
    }
    const QJsonObject o = doc.object();
    if (!o.value(QLatin1String("configured")).toBool()) {
        return s;
    }
    s.configured = true;
    s.running = slotLetter(o.value(QLatin1String("running")));
    if (s.running.isEmpty()) {
        s.running = slotLetter(o.value(QLatin1String("active")));
    }
    s.next = slotLetter(o.value(QLatin1String("next")));
    s.attempts = o.value(QLatin1String("attempts")).toInt();
    const QString trying = slotLetter(o.value(QLatin1String("trying")));
    s.trial = !trying.isEmpty() && trying == s.running;
    if (s.running == QLatin1String("A") || s.running == QLatin1String("B")) {
        s.other = s.running == QLatin1String("A") ? QStringLiteral("B") : QStringLiteral("A");
        const QJsonObject slots = o.value(QLatin1String("slots")).toObject();
        const QJsonObject mine = slots.value(s.running.toLower()).toObject();
        const QJsonObject theirs = slots.value(s.other.toLower()).toObject();
        s.runningVersion = mine.value(QLatin1String("version")).toString();
        s.runningState = mine.value(QLatin1String("state")).toString();
        s.otherVersion = theirs.value(QLatin1String("version")).toString();
        s.otherState = theirs.value(QLatin1String("state")).toString();
    }
    return s;
}

} // namespace Kdos

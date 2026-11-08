/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

#include "boxprofile.h"

#include <KConfigGroup>
#include <KFormat>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QThread>

#include <cmath>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
/* The page's state file: create-time changes still waiting for the box to
 * be made again, one group per box. It is the page's own memory, not a
 * second profile: nothing but this page reads it. */
const auto StateFile = u"kcm_kdos_boxesstaterc"_s;

bool truthy(const QString &v)
{
    return v == "private"_L1 || v == "yes"_L1 || v == "on"_L1 || v == "1"_L1 || v == "true"_L1;
}

const QStringList SharedKeys{u"ipc"_s, u"devices"_s, u"processes"_s, u"home"_s};
const QStringList YesNoKeys{u"wayland"_s, u"audio"_s, u"gpu"_s};

/* Keys of `kdos-box profile` output that the page shows a control for; a
 * `!` line under any other key is shown at the top of the page. */
bool hasControl(const QString &key)
{
    return BoxProfile::editableKeys().contains(key);
}
}

QString boxProfileDir()
{
    return QDir::homePath() + u"/.config/kdos/boxes"_s;
}

BoxProfile::BoxProfile(QObject *parent)
    : QObject(parent)
    , m_values(QQmlPropertyMap::create(this))
    , m_mechanisms(QQmlPropertyMap::create(this))
    , m_notes(QQmlPropertyMap::create(this))
{
    connect(m_values, &QQmlPropertyMap::valueChanged, this, &BoxProfile::recomputeDirty);
    setValues(defaultValues());
    m_saved = defaultValues();
}

QStringList BoxProfile::editableKeys()
{
    return {u"accent"_s,
            u"grant"_s,
            u"persistence"_s,
            u"network"_s,
            u"ipc"_s,
            u"devices"_s,
            u"processes"_s,
            u"home"_s,
            u"init"_s,
            u"wayland"_s,
            u"audio"_s,
            u"gpu"_s,
            u"x11"_s,
            u"render"_s,
            u"memory"_s,
            u"cpus"_s,
            u"pids"_s,
            u"autostop"_s};
}

QStringList BoxProfile::createTimeKeys()
{
    /* Namespaces (network, ipc, devices, processes), volumes (home, gpu's
     * /dev/dri), the init and the podman limits are flags of `create`.
     * Accent, grant, render, wayland, audio, x11 and autostop are read at
     * each launch — the private runtime directory is filled again for every
     * one — or by KWin and kdos-box gc, and apply without a recreate. */
    return {u"persistence"_s,
            u"network"_s,
            u"ipc"_s,
            u"devices"_s,
            u"processes"_s,
            u"home"_s,
            u"init"_s,
            u"gpu"_s,
            u"memory"_s,
            u"cpus"_s,
            u"pids"_s};
}

QVariantMap BoxProfile::defaultValues()
{
    return {
        {u"accent"_s, u"session"_s},
        {u"grant"_s, QStringList()},
        {u"persistence"_s, u"persistent"_s},
        {u"network"_s, u"host"_s},
        {u"ipc"_s, u"shared"_s},
        {u"devices"_s, u"shared"_s},
        {u"processes"_s, u"shared"_s},
        {u"home"_s, u"shared"_s},
        {u"init"_s, u"no"_s},
        {u"wayland"_s, u"yes"_s},
        {u"audio"_s, u"yes"_s},
        {u"gpu"_s, u"yes"_s},
        {u"x11"_s, u"auto"_s},
        {u"render"_s, u"auto"_s},
        {u"memory"_s, 0},
        {u"cpus"_s, 0},
        {u"pids"_s, 0},
        {u"autostop"_s, 0},
    };
}

QStringList BoxProfile::grantVocabulary()
{
    return {u"window-list"_s,
            u"fake-input"_s,
            u"screencast"_s,
            u"data-control"_s,
            u"layer-shell"_s,
            u"output-management"_s,
            u"dpms"_s,
            u"plasma-shell"_s,
            u"input-method"_s,
            u"secrets"_s};
}

int BoxProfile::parseMemoryMiB(const QString &raw)
{
    static const QRegularExpression re(u"^\\s*([0-9]+(?:\\.[0-9]+)?)\\s*([bkmgt]?)i?b?\\s*$"_s, QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(raw);
    if (!m.hasMatch()) {
        return 0;
    }
    const double n = m.captured(1).toDouble();
    const QString unit = m.captured(2).toLower();
    double mib = 0;
    if (unit.isEmpty() || unit == "b"_L1) {
        mib = n / (1024.0 * 1024.0);    /* podman reads a bare number as bytes */
    } else if (unit == "k"_L1) {
        mib = n / 1024.0;
    } else if (unit == "m"_L1) {
        mib = n;
    } else if (unit == "g"_L1) {
        mib = n * 1024.0;
    } else if (unit == "t"_L1) {
        mib = n * 1024.0 * 1024.0;
    }
    if (n > 0 && mib < 1) {
        return 1;
    }
    return int(std::lround(mib));
}

QString BoxProfile::memorySpelling(int mib)
{
    if (mib <= 0) {
        return QString();
    }
    if (mib % 1024 == 0) {
        return QString::number(mib / 1024) + u'G';
    }
    return QString::number(mib) + u'M';
}

int BoxProfile::parseCpuTenths(const QString &raw)
{
    bool ok = false;
    const double n = raw.trimmed().toDouble(&ok);
    if (!ok || n <= 0) {
        return 0;
    }
    return int(std::lround(n * 10.0));
}

QString BoxProfile::cpuSpelling(int tenths)
{
    if (tenths <= 0) {
        return QString();
    }
    if (tenths % 10 == 0) {
        return QString::number(tenths / 10);
    }
    return QString::number(tenths / 10) + u'.' + QString::number(tenths % 10);
}

int BoxProfile::parseDurationSeconds(const QString &raw)
{
    /* profile_set()'s duration(): a number and an optional unit, h, m or
     * anything else for seconds. */
    static const QRegularExpression re(u"^\\s*(-?[0-9]+)\\s*([a-zA-Z]?)"_s);
    const auto m = re.match(raw);
    if (!m.hasMatch()) {
        return 0;
    }
    const long long n = m.captured(1).toLongLong();
    if (n <= 0) {
        return 0;
    }
    const QString unit = m.captured(2).toLower();
    if (unit == "h"_L1) {
        return int(n * 3600);
    }
    if (unit == "m"_L1) {
        return int(n * 60);
    }
    return int(n);
}

QVariant BoxProfile::normalize(const QString &key, const QString &raw)
{
    const QString v = raw.trimmed();
    if (key == "accent"_L1) {
        return v.isEmpty() ? u"session"_s : v;
    }
    if (key == "grant"_L1) {
        return v.split(QRegularExpression(u"[,\\s]+"_s), Qt::SkipEmptyParts);
    }
    if (key == "persistence"_L1) {
        return (v == "ephemeral"_L1 || v == "frozen"_L1) ? v : u"persistent"_s;
    }
    if (key == "network"_L1) {
        if (v == "none"_L1) {
            return u"none"_s;
        }
        return truthy(v) ? u"private"_s : u"host"_s;
    }
    if (SharedKeys.contains(key)) {
        return truthy(v) ? u"private"_s : u"shared"_s;
    }
    if (key == "init"_L1) {
        return truthy(v) ? u"yes"_s : u"no"_s;
    }
    if (YesNoKeys.contains(key)) {
        return (truthy(v) || v == "shared"_L1) ? u"yes"_s : u"no"_s;
    }
    if (key == "x11"_L1) {
        /* `auto` is the default and anything unrecognised, so a typo
         * leaves the decision to the box's application rather than
         * handing it the display. */
        static const QStringList refused{u"no"_s, u"off"_s, u"0"_s, u"false"_s};
        if (refused.contains(v)) {
            return u"no"_s;
        }
        return truthy(v) ? u"yes"_s : u"auto"_s;
    }
    if (key == "render"_L1) {
        static const QStringList software{u"software"_s, u"pixman"_s, u"no"_s, u"off"_s, u"0"_s, u"false"_s};
        if (software.contains(v)) {
            return u"software"_s;
        }
        return v == "gpu"_L1 ? u"gpu"_s : u"auto"_s;
    }
    if (key == "memory"_L1) {
        return parseMemoryMiB(v);
    }
    if (key == "cpus"_L1) {
        return parseCpuTenths(v);
    }
    if (key == "pids"_L1) {
        return qMax(0, v.toInt());
    }
    if (key == "autostop"_L1) {
        return parseDurationSeconds(v);
    }
    return v;
}

QString BoxProfile::spell(const QString &key, const QVariant &value)
{
    if (key == "grant"_L1) {
        /* The known names in the vocabulary's order, then any other name
         * the file held, so a name this page has no box for survives. */
        const QStringList names = value.toStringList();
        QStringList out;
        for (const QString &g : grantVocabulary()) {
            if (names.contains(g)) {
                out << g;
            }
        }
        for (const QString &g : names) {
            if (!out.contains(g)) {
                out << g;
            }
        }
        return out.join(u',');
    }
    if (key == "memory"_L1) {
        return memorySpelling(value.toInt());
    }
    if (key == "cpus"_L1) {
        return cpuSpelling(value.toInt());
    }
    if (key == "pids"_L1) {
        return QString::number(qMax(0, value.toInt()));
    }
    if (key == "autostop"_L1) {
        const int s = qMax(0, value.toInt());
        return s ? QString::number(s) + u's' : u"0"_s;
    }
    return value.toString();
}

void BoxProfile::setValues(const QVariantMap &v)
{
    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        m_values->insert(it.key(), it.value());
    }
}

void BoxProfile::loadFile(const QString &name)
{
    /* Another box's report says nothing about this one: until this box's
     * arrives, no note is shown. The same box reloaded keeps its report,
     * which the caller refreshes. */
    if (name != m_name) {
        for (const QString &key : editableKeys()) {
            m_notes->insert(key, QStringList());
            m_mechanisms->insert(key, QString());
        }
        m_generalNotes.clear();
        m_notesLoaded = false;
        Q_EMIT notesChanged();
    }
    m_name = name;
    QVariantMap vals = defaultValues();
    m_described = false;
    m_otherGrants.clear();

    QFile f(boxProfileDir() + u'/' + name + u".conf"_s);
    if (f.open(QIODevice::ReadOnly)) {
        m_described = true;
        const auto lines = QString::fromUtf8(f.readAll()).split(u'\n');
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith(u'#')) {
                continue;
            }
            const int eq = line.indexOf(u'=');
            if (eq < 0) {
                continue;
            }
            const QString key = line.left(eq).trimmed();
            if (vals.contains(key)) {
                vals.insert(key, normalize(key, line.mid(eq + 1)));
            }
        }
    }
    const QStringList vocab = grantVocabulary();
    for (const QString &g : vals.value(u"grant"_s).toStringList()) {
        if (!vocab.contains(g)) {
            m_otherGrants << g;
        }
    }

    m_saved = vals;
    setValues(vals);
    loadPending();
    recomputeDirty();
    Q_EMIT loaded();
}

void BoxProfile::applyReport(const QString &report)
{
    static const QRegularExpression keyLine(u"^([a-z][a-z0-9-]*)\\s*= ?(.*)$"_s);
    static const QRegularExpression noteLine(u"^\\s+!\\s*(.*)$"_s);

    QMap<QString, QStringList> notes;
    QStringList general;
    QMap<QString, QString> mech;
    QString last;

    const auto lines = report.split(u'\n');
    for (const QString &line : lines) {
        const auto n = noteLine.match(line);
        if (n.hasMatch()) {
            const QString text = n.captured(1).trimmed();
            if (text.isEmpty()) {
                continue;
            }
            /* A note about a key kdos-box could not parse belongs to no
             * control; nor does one under a key the page does not show. */
            if (last.isEmpty() || !hasControl(last) || text.contains(u"unknown key"_s)) {
                general << text;
            } else {
                notes[last] << text;
                /* The one note that is about two keys names both. */
                if (last == "gpu"_L1 && text.contains(u"audio="_s)) {
                    notes[u"audio"_s] << text;
                }
            }
            continue;
        }
        const auto k = keyLine.match(line);
        if (!k.hasMatch()) {
            continue;
        }
        last = k.captured(1);
        if (last == "accent"_L1 || last == "grant"_L1 || last == "base"_L1 || last == "box"_L1 || last == "image"_L1) {
            continue;
        }
        /* `key = <value padded to 11> <what enforces it>`: every key with a
         * mechanism has a one-word value. */
        const QString rest = k.captured(2).trimmed();
        const int sp = rest.indexOf(u' ');
        mech.insert(last, sp < 0 ? QString() : rest.mid(sp + 1).trimmed());
    }

    for (const QString &key : editableKeys()) {
        m_notes->insert(key, notes.value(key));
        m_mechanisms->insert(key, mech.value(key));
    }
    m_generalNotes = general;
    m_notesLoaded = true;
    Q_EMIT notesChanged();
}

QString BoxProfile::name() const
{
    return m_name;
}

bool BoxProfile::isDescribed() const
{
    return m_described;
}

QQmlPropertyMap *BoxProfile::values() const
{
    return m_values;
}

QQmlPropertyMap *BoxProfile::mechanisms() const
{
    return m_mechanisms;
}

QQmlPropertyMap *BoxProfile::notes() const
{
    return m_notes;
}

QStringList BoxProfile::generalNotes() const
{
    return m_generalNotes;
}

bool BoxProfile::notesLoaded() const
{
    return m_notesLoaded;
}

QStringList BoxProfile::otherGrants() const
{
    return m_otherGrants;
}

bool BoxProfile::isDirty() const
{
    return m_dirty;
}

bool BoxProfile::isAtDefaults() const
{
    return m_atDefaults;
}

QStringList BoxProfile::changedKeys() const
{
    QStringList out;
    for (const QString &key : editableKeys()) {
        /* Compared spelled, so `4G` against 4096 MiB, or a grant list in
         * another order, is not a change. */
        if (spell(key, m_values->value(key)) != spell(key, m_saved.value(key))) {
            out << key;
        }
    }
    return out;
}

QStringList BoxProfile::changedArguments() const
{
    QStringList out;
    for (const QString &key : changedKeys()) {
        out << key + u'=' + spell(key, m_values->value(key));
    }
    return out;
}

void BoxProfile::recomputeDirty()
{
    const bool dirty = !changedKeys().isEmpty();
    const QVariantMap defs = defaultValues();
    bool atDefaults = true;
    for (const QString &key : editableKeys()) {
        if (spell(key, m_values->value(key)) != spell(key, defs.value(key))) {
            atDefaults = false;
            break;
        }
    }
    if (dirty != m_dirty || atDefaults != m_atDefaults) {
        m_dirty = dirty;
        m_atDefaults = atDefaults;
        Q_EMIT dirtyChanged();
    }
}

void BoxProfile::resetToDefaults()
{
    /* A grant this page has no box for is kept: Defaults resets what the
     * page shows, and a name it does not show is not its to drop. */
    QVariantMap defs = defaultValues();
    defs.insert(u"grant"_s, m_otherGrants);
    setValues(defs);
    recomputeDirty();
    Q_EMIT loaded();
}

void BoxProfile::discardEdits()
{
    setValues(m_saved);
    recomputeDirty();
    Q_EMIT loaded();
}

QStringList BoxProfile::pendingRecreate() const
{
    return m_pending;
}

void BoxProfile::loadPending()
{
    const KConfigGroup g(KSharedConfig::openStateConfig(StateFile), m_name);
    m_pending = g.readEntry("PendingKeys", QStringList());
    const qint64 since = g.readEntry("Since", qint64(0));
    m_pendingSince = since > 0 ? QDateTime::fromSecsSinceEpoch(since) : QDateTime();
    Q_EMIT pendingChanged();
}

void BoxProfile::savePending(const QStringList &keys, const QDateTime &since)
{
    KSharedConfigPtr cfg = KSharedConfig::openStateConfig(StateFile);
    KConfigGroup g(cfg, m_name);
    if (keys.isEmpty()) {
        g.deleteGroup();
    } else {
        g.writeEntry("PendingKeys", keys);
        g.writeEntry("Since", since.toSecsSinceEpoch());
    }
    cfg->sync();
    m_pending = keys;
    m_pendingSince = since;
    Q_EMIT pendingChanged();
}

void BoxProfile::addPendingRecreate(const QStringList &keys)
{
    if (m_name.isEmpty() || keys.isEmpty()) {
        return;
    }
    recordPendingRecreate(m_name, keys);
    loadPending();
}

void BoxProfile::recordPendingRecreate(const QString &box, const QStringList &keys)
{
    if (box.isEmpty() || keys.isEmpty()) {
        return;
    }
    KSharedConfigPtr cfg = KSharedConfig::openStateConfig(StateFile);
    KConfigGroup g(cfg, box);
    QStringList merged = g.readEntry("PendingKeys", QStringList());
    for (const QString &k : keys) {
        if (!merged.contains(k)) {
            merged << k;
        }
    }
    g.writeEntry("PendingKeys", merged);
    if (g.readEntry("Since", qint64(0)) <= 0) {
        g.writeEntry("Since", QDateTime::currentDateTime().toSecsSinceEpoch());
    }
    cfg->sync();
}

void BoxProfile::clearPendingRecreate()
{
    if (!m_name.isEmpty() && !m_pending.isEmpty()) {
        savePending({}, QDateTime());
    }
}

void BoxProfile::reconcilePending(bool hasContainer, const QDateTime &created)
{
    if (m_pending.isEmpty()) {
        return;
    }
    if (!hasContainer || (created.isValid() && m_pendingSince.isValid() && created > m_pendingSince)) {
        clearPendingRecreate();
    }
}

QString BoxProfile::formatDuration(int seconds) const
{
    if (seconds <= 0) {
        return i18nc("@item:inlistbox stop an idle box", "Never");
    }
    return KFormat().formatSpelloutDuration(quint64(seconds) * 1000);
}

int BoxProfile::cpuCount() const
{
    return qMax(1, QThread::idealThreadCount());
}

QString BoxProfile::formatCpus(int tenths) const
{
    if (tenths <= 0) {
        return i18nc("@item cpu limit", "All cores");
    }
    if (tenths % 10 == 0) {
        return i18ncp("@item cpu limit", "%1 core", "%1 cores", tenths / 10);
    }
    return i18nc("@item cpu limit, %1 a fractional number of cores", "%1 cores", cpuSpelling(tenths));
}

int BoxProfile::memoryFromText(const QString &text) const
{
    QString t = text;
    t.replace(u',', u'.');
    return parseMemoryMiB(t);
}

int BoxProfile::durationFromText(const QString &text) const
{
    return parseDurationSeconds(text);
}

int BoxProfile::cpusFromText(const QString &text) const
{
    QString t = text.trimmed();
    t.replace(u',', u'.');
    const int sp = t.indexOf(u' ');
    return parseCpuTenths(sp < 0 ? t : t.left(sp));
}

QString BoxProfile::formatMemory(int mib) const
{
    if (mib <= 0) {
        return i18nc("@item memory limit", "No limit");
    }
    return KFormat().formatByteSize(double(mib) * 1024.0 * 1024.0, 1, KFormat::IECBinaryDialect);
}

}

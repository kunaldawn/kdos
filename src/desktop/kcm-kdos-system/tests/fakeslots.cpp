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
 *
 * The fixture's answers are what kdos-sysupdated's must be: the same keys,
 * the same precedence for `state`, the same refusals in kdos-bootctl's words.
 */

#include "fakeslots.h"

#include "slots1adaptor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
const auto Path = u"/io/kdos/Slots1"_s;
const auto Iface = u"io.kdos.Slots1"_s;
const auto Refused = u"io.kdos.Slots1.Error.Refused"_s;

QString letter(int i)
{
    return i == 1 ? u"b"_s : u"a"_s;
}

int indexOf(const QString &s)
{
    if (s == "a"_L1 || s == "A"_L1) {
        return 0;
    }
    if (s == "b"_L1 || s == "B"_L1) {
        return 1;
    }
    return -1;
}

/* `key = value` lines, `#` comments: the layout of the boot state, of
 * os-release (quotes stripped) and of update.conf. */
QMap<QString, QString> keyValues(const QString &text, QChar sep)
{
    QMap<QString, QString> out;
    const auto lines = text.split(u'\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#')) {
            continue;
        }
        const int eq = line.indexOf(sep);
        if (eq < 0) {
            continue;
        }
        QString v = line.mid(eq + 1).trimmed();
        if (v.size() >= 2 && v.startsWith(u'"') && v.endsWith(u'"')) {
            v = v.mid(1, v.size() - 2);
        }
        out.insert(line.left(eq).trimmed(), v);
    }
    return out;
}
}

FakeSlots::FakeSlots(const QString &dir, QObject *parent)
    : QObject(parent)
    , m_dir(dir)
{
}

QString FakeSlots::readFile(const QString &name) const
{
    QFile f(m_dir + u'/' + name);
    if (!f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

FakeSlots::Boot FakeSlots::readBoot() const
{
    Boot b;
    const QString text = readFile(u"bootstate"_s);
    if (text.isEmpty()) {
        return b;
    }
    const auto kv = keyValues(text, u'=');
    if (!kv.contains(u"active"_s) || indexOf(kv.value(u"active"_s)) < 0) {
        return b;
    }
    b.have = true;
    b.slot[0] = kv.value(u"slot_a"_s);
    b.slot[1] = kv.value(u"slot_b"_s);
    b.crypt[0] = kv.value(u"crypt_a"_s);
    b.crypt[1] = kv.value(u"crypt_b"_s);
    b.active = indexOf(kv.value(u"active"_s));
    b.trying = indexOf(kv.value(u"try"_s));
    b.attempts = kv.value(u"attempts"_s).toInt();
    b.bootnext = kv.value(u"bootnext"_s) == "yes"_L1;
    if (b.trying >= 0 && b.slot[b.trying].isEmpty()) {
        b.trying = -1;
    }
    return b;
}

bool FakeSlots::writeBoot(const Boot &b) const
{
    QSaveFile f(m_dir + u"/bootstate"_s);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QString text = QStringLiteral(
                             "# KDOS boot state. Written by kdos-bootctl; the initramfs\n"
                             "# reads it before it mounts a root filesystem.\n"
                             "slot_a   = %1\n"
                             "slot_b   = %2\n"
                             "crypt_a  = %3\n"
                             "crypt_b  = %4\n"
                             "active   = %5\n"
                             "try      = %6\n"
                             "attempts = %7\n"
                             "bootnext = %8\n")
                             .arg(b.slot[0],
                                  b.slot[1],
                                  b.crypt[0],
                                  b.crypt[1],
                                  letter(b.active),
                                  b.trying >= 0 ? letter(b.trying) : QString(),
                                  QString::number(b.attempts),
                                  b.trying >= 0 && b.bootnext ? u"yes"_s : QString());
    f.write(text.toUtf8());
    return f.commit();
}

bool FakeSlots::denied(const QString &method)
{
    const auto lines = readFile(u"deny"_s).split(u'\n');
    for (const QString &l : lines) {
        if (l.trimmed() == method) {
            sendErrorReply(u"org.freedesktop.DBus.Error.AccessDenied"_s, u"Not authorized to perform operation"_s);
            return true;
        }
    }
    return false;
}

void FakeSlots::log(const QString &line) const
{
    QFile f(m_dir + u"/calls.log"_s);
    if (f.open(QIODevice::Append)) {
        f.write(line.toUtf8() + '\n');
    }
}

void FakeSlots::announce()
{
    QVariantMap all;
    all.insert(u"Configured"_s, configured());
    all.insert(u"Live"_s, live());
    all.insert(u"RunningSlot"_s, runningSlot());
    all.insert(u"ActiveSlot"_s, activeSlot());
    all.insert(u"TrySlot"_s, trySlot());
    all.insert(u"TryAttempts"_s, tryAttempts());
    all.insert(u"TryBootNext"_s, tryBootNext());
    all.insert(u"Channel"_s, channel());
    all.insert(u"InstallRoute"_s, installRoute());
    all.insert(u"BuildDeps"_s, buildDeps());
    all.insert(u"State"_s, state());
    QDBusMessage sig = QDBusMessage::createSignal(Path, u"org.freedesktop.DBus.Properties"_s, u"PropertiesChanged"_s);
    sig << Iface << all << QStringList();
    QDBusConnection::sessionBus().send(sig);
    Q_EMIT SlotsChanged();
}

bool FakeSlots::configured() const
{
    return readBoot().have;
}

bool FakeSlots::live() const
{
    return !configured() && QFile::exists(m_dir + u"/live"_s);
}

QString FakeSlots::runningSlot() const
{
    static const QRegularExpression re(u"(?:^|\\s)kdos_slot=([ab])(?:\\s|$)"_s);
    const auto m = re.match(readFile(u"cmdline"_s));
    return m.hasMatch() ? m.captured(1) : QString();
}

QString FakeSlots::activeSlot() const
{
    const Boot b = readBoot();
    return b.have ? letter(b.active) : QString();
}

QString FakeSlots::trySlot() const
{
    const Boot b = readBoot();
    return b.have && b.trying >= 0 ? letter(b.trying) : QString();
}

uint FakeSlots::tryAttempts() const
{
    const Boot b = readBoot();
    return b.have && b.trying >= 0 ? uint(b.attempts) : 0;
}

bool FakeSlots::tryBootNext() const
{
    const Boot b = readBoot();
    return b.have && b.trying >= 0 && b.bootnext;
}

QString FakeSlots::channel() const
{
    return keyValues(readFile(u"update.conf"_s), u'=').value(u"tree"_s);
}

/* kdos-sysupdated's state_route(): "source" when update.conf says so, else
 * "prebuilt". */
static QString route(const QString &conf, const QString &key)
{
    return keyValues(conf, u'=').value(key) == "source"_L1 ? u"source"_s : u"prebuilt"_s;
}

QString FakeSlots::installRoute() const
{
    return route(readFile(u"update.conf"_s), u"install"_s);
}

QString FakeSlots::buildDeps() const
{
    return route(readFile(u"update.conf"_s), u"build-deps"_s);
}

QVariantMap FakeSlots::state() const
{
    QVariantMap st;
    const Boot b = readBoot();
    const QString running = runningSlot();
    const QString trying = b.have && b.trying >= 0 ? letter(b.trying) : QString();
    const QStringList job = readFile(u"job"_s).simplified().split(u' ', Qt::SkipEmptyParts);

    st.insert(u"job"_s, job.value(0));
    st.insert(u"jobDone"_s, job.value(1).toUInt());
    st.insert(u"jobTotal"_s, job.value(2).toUInt());
    const uint total = job.value(2).toUInt();
    st.insert(u"jobProgress"_s, total ? job.value(1).toUInt() * 100 / total : 0u);
    st.insert(u"rebootRequired"_s, !trying.isEmpty() && trying != running);
    st.insert(u"onTrial"_s, !trying.isEmpty() && trying == running);
    st.insert(u"blessUserspace"_s, QFile::exists(m_dir + u"/bless-userspace"_s));
    st.insert(u"blessGraphical"_s, QFile::exists(m_dir + u"/bless-graphical"_s));
    st.insert(u"rolledBack"_s, readFile(u"rolledback"_s).trimmed());

    /* kdos-sysupdated's state_recompute() and state_current_offer(), in
     * their order: the job, the running trial, the armed one, a prepared
     * slot, and otherwise nothing. */
    const QString offer = readFile(u"offer"_s).trimmed();
    const QStringList acquired = readFile(u"acquired"_s).simplified().split(u' ', Qt::SkipEmptyParts);
    QString phase = u"idle"_s;
    QString version;
    if (!job.isEmpty()) {
        phase = job.value(0) == "install"_L1 ? u"installing"_s : u"acquiring"_s;
        version = offer;
    } else if (!trying.isEmpty() && trying == running) {
        phase = u"trial"_s;
        version = slotVersion(b.trying);
    } else if (!trying.isEmpty()) {
        phase = u"armed"_s;
        version = slotVersion(b.trying);
    } else if (acquired.size() == 2) {
        phase = u"acquired"_s;
        version = acquired.at(1);
    }
    const bool armedWithOffer = phase == "armed"_L1 && version == offer;
    st.insert(u"phase"_s, phase);
    st.insert(u"version"_s, version);
    st.insert(u"offer"_s, armedWithOffer ? QString() : offer);
    st.insert(u"error"_s, readFile(u"error"_s).trimmed());
    return st;
}

QString FakeSlots::slotVersion(int i) const
{
    if (i < 0 || i > 1) {
        return QString();
    }
    return keyValues(readFile(u"slot-"_s + letter(i) + u"/etc/os-release"_s), u'=').value(u"VERSION_ID"_s);
}

QVariantMap FakeSlots::slotMap(const Boot &b, int i) const
{
    const QString id = letter(i);
    const QString root = u"slot-"_s + id;
    const auto osr = keyValues(readFile(root + u"/etc/os-release"_s), u'=');
    const auto tree = keyValues(readFile(root + u"/ports/TREE"_s), u' ');
    const QString kernel = readFile(u"kernel-"_s + id).trimmed();
    const QString version = osr.value(u"VERSION_ID"_s);
    const bool present = !b.slot[i].isEmpty();
    const bool confirmed = b.active == i;
    const bool trial = b.trying == i;
    const bool failed = readFile(u"rolledback"_s).trimmed() == id;

    QString reason;
    if (!present) {
        reason = u"This slot has no system in it."_s;
    } else if (confirmed) {
        reason = u"This is already the version the computer starts."_s;
    } else if (kernel.isEmpty()) {
        reason = u"Its kernel is not on the boot partition."_s;
    }

    QString state = u"empty"_s;
    if (trial) {
        state = u"trial"_s;
    } else if (failed) {
        state = u"failed"_s;
    } else if (confirmed) {
        state = u"confirmed"_s;
    } else if (present) {
        state = u"installed"_s;
    }

    QVariantMap m;
    m.insert(u"id"_s, id);
    m.insert(u"title"_s, version.isEmpty() ? u"Slot %1"_s.arg(id.toUpper()) : u"KDOS %1 (slot %2)"_s.arg(version, id.toUpper()));
    m.insert(u"present"_s, present);
    m.insert(u"version"_s, present ? version : QString());
    m.insert(u"buildTime"_s, qlonglong(present ? tree.value(u"time"_s).toLongLong() : 0));
    m.insert(u"treeCommit"_s, present ? tree.value(u"commit"_s) : QString());
    m.insert(u"kernel"_s, present ? kernel : QString());
    m.insert(u"uuid"_s, b.slot[i]);
    m.insert(u"encrypted"_s, !b.crypt[i].isEmpty());
    m.insert(u"running"_s, runningSlot() == id);
    m.insert(u"confirmed"_s, confirmed);
    m.insert(u"trial"_s, trial);
    m.insert(u"failed"_s, failed);
    m.insert(u"bootable"_s, reason.isEmpty() && !trial);
    m.insert(u"reason"_s, reason);
    m.insert(u"state"_s, state);
    return m;
}

QList<QVariantMap> FakeSlots::ListSlots()
{
    const Boot b = readBoot();
    if (!b.have) {
        return {};
    }
    return {slotMap(b, 0), slotMap(b, 1)};
}

void FakeSlots::Try(const QString &slot)
{
    if (denied(u"Try"_s)) {
        return;
    }
    const int i = indexOf(slot);
    if (i < 0) {
        sendErrorReply(u"org.freedesktop.DBus.Error.InvalidArgs"_s, u"no slot named '%1'"_s.arg(slot));
        return;
    }
    Boot b = readBoot();
    if (!b.have) {
        sendErrorReply(Refused, u"bootctl: no boot state to update"_s);
        return;
    }
    if (b.slot[i].isEmpty()) {
        sendErrorReply(Refused, u"bootctl: slot %1 has no root filesystem"_s.arg(letter(i)));
        return;
    }
    if (i == b.active) {
        sendErrorReply(Refused, u"bootctl: slot %1 is already active"_s.arg(letter(i)));
        return;
    }
    if (readFile(u"kernel-"_s + letter(i)).trimmed().isEmpty()) {
        sendErrorReply(Refused, u"bootctl: slot %1 has no kernel on the ESP"_s.arg(letter(i)));
        return;
    }
    /* `kdos-bootctl try <slot> 1` on a machine led by the menu: one
     * counted boot, no BootNext. */
    b.trying = i;
    b.attempts = 1;
    b.bootnext = false;
    if (!writeBoot(b)) {
        sendErrorReply(Refused, u"bootctl: cannot write the boot state"_s);
        return;
    }
    log(u"Try "_s + letter(i));
    announce();
}

void FakeSlots::CancelTry()
{
    if (denied(u"CancelTry"_s)) {
        return;
    }
    Boot b = readBoot();
    if (!b.have || b.trying < 0) {
        log(u"CancelTry"_s);
        return;
    }
    if (letter(b.trying) == runningSlot()) {
        sendErrorReply(Refused, u"bootctl: slot %1 is running on trial; it is confirmed or rolled back, not cancelled"_s.arg(letter(b.trying)));
        return;
    }
    b.trying = -1;
    b.attempts = 0;
    b.bootnext = false;
    writeBoot(b);
    log(u"CancelTry"_s);
    announce();
}

void FakeSlots::SetChannel(const QString &channel)
{
    if (denied(u"SetChannel"_s)) {
        return;
    }
    if (!channel.isEmpty()) {
        const QUrl url(channel, QUrl::StrictMode);
        const QString scheme = url.scheme();
        const bool path = channel.startsWith(u'/');
        const bool web = url.isValid() && (scheme == "https"_L1 || scheme == "http"_L1 || scheme == "file"_L1);
        static const QRegularExpression space(u"\\s"_s);
        if ((!path && !web) || channel.contains(space)) {
            sendErrorReply(u"org.freedesktop.DBus.Error.InvalidArgs"_s, u"not an update channel: %1"_s.arg(channel));
            return;
        }
    }
    QStringList kept;
    const auto lines = readFile(u"update.conf"_s).split(u'\n');
    for (const QString &l : lines) {
        if (l.trimmed().startsWith(u"tree"_s) && l.contains(u'=')) {
            continue;
        }
        if (!l.isEmpty()) {
            kept << l;
        }
    }
    if (!channel.isEmpty()) {
        kept << u"tree = "_s + channel;
    }
    QSaveFile f(m_dir + u"/update.conf"_s);
    if (!f.open(QIODevice::WriteOnly)) {
        sendErrorReply(Refused, u"cannot write update.conf"_s);
        return;
    }
    f.write((kept.join(u'\n') + (kept.isEmpty() ? QString() : u"\n"_s)).toUtf8());
    f.commit();
    log(u"SetChannel "_s + channel);
    announce();
}

void FakeSlots::SetInstallRoute(const QString &install, const QString &deps)
{
    if (denied(u"SetInstallRoute"_s)) {
        return;
    }
    for (const QString &v : {install, deps}) {
        if (v != "prebuilt"_L1 && v != "source"_L1) {
            sendErrorReply(u"org.freedesktop.DBus.Error.InvalidArgs"_s, u"a route is prebuilt or source"_s);
            return;
        }
    }
    QStringList kept;
    const auto lines = readFile(u"update.conf"_s).split(u'\n');
    for (const QString &l : lines) {
        const QString k = l.section(u'=', 0, 0).trimmed();
        if (l.contains(u'=') && (k == "install"_L1 || k == "build-deps"_L1)) {
            continue;
        }
        if (!l.isEmpty()) {
            kept << l;
        }
    }
    kept << u"install = "_s + install << u"build-deps = "_s + deps;
    QSaveFile f(m_dir + u"/update.conf"_s);
    if (!f.open(QIODevice::WriteOnly)) {
        sendErrorReply(Refused, u"cannot write update.conf"_s);
        return;
    }
    f.write((kept.join(u'\n') + u"\n"_s).toUtf8());
    f.commit();
    log(u"SetInstallRoute "_s + install + u' ' + deps);
    announce();
}

void FakeSlots::Bless()
{
    if (denied(u"Bless"_s)) {
        return;
    }
    QFile f(m_dir + u"/bless-graphical"_s);
    if (!f.open(QIODevice::WriteOnly)) {
        sendErrorReply(Refused, u"cannot write the graphical marker"_s);
        return;
    }
    f.close();
    log(u"Bless"_s);
    announce();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        qCritical("usage: fakeslots1 <fixture-dir>");
        return 2;
    }
    qDBusRegisterMetaType<QList<QVariantMap>>();

    FakeSlots server(QDir(QString::fromLocal8Bit(argv[1])).absolutePath());
    new Slots1Adaptor(&server);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("fakeslots1: no session bus");
        return 1;
    }
    if (!bus.registerObject(Path, &server) || !bus.registerService(Iface)) {
        qCritical("fakeslots1: cannot own io.kdos.Slots1 on the session bus");
        return 1;
    }
    return app.exec();
}

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

#include "slotinfo.h"

#include <KLocalizedString>

#include <QLocale>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
/* A slot an update stopped writing: its root is part old tree, part new,
 * and the ESP's label for it still names the old version. Either key
 * marks it, so a server that sends only one is still read right. */
bool incomplete(const QVariantMap &map)
{
    return map.value(u"incomplete"_s).toBool() || map.value(u"state"_s).toString() == "incomplete"_L1;
}
}

SlotInfo::SlotInfo(const QString &slotId, QObject *parent)
    : QObject(parent)
    , m_id(slotId)
{
}

void SlotInfo::update(const QVariantMap &map)
{
    if (map == m_map) {
        return;
    }
    m_map = map;
    Q_EMIT changed();
}

void SlotInfo::clear()
{
    update({});
}

QString SlotInfo::slotId() const
{
    return m_id;
}

QString SlotInfo::letter() const
{
    return m_id.toUpper();
}

QString SlotInfo::title() const
{
    /* The server's title and the root's version describe what the slot
     * held before the update began writing it, which is no longer there. */
    if (incomplete(m_map)) {
        return i18nc("@title %1 slot letter", "Slot %1", letter());
    }
    const QString t = m_map.value(u"title"_s).toString();
    if (!t.isEmpty()) {
        return t;
    }
    if (!version().isEmpty()) {
        return i18nc("@title %1 version, %2 slot letter", "KDOS %1 (slot %2)", version(), letter());
    }
    return i18nc("@title %1 slot letter", "Slot %1", letter());
}

bool SlotInfo::isPresent() const
{
    return m_map.value(u"present"_s).toBool();
}

QString SlotInfo::version() const
{
    return m_map.value(u"version"_s).toString();
}

QString SlotInfo::versionText() const
{
    if (!isPresent()) {
        return i18nc("@info a slot with no system in it", "Nothing installed");
    }
    /* The record's version is the system the update began overwriting. */
    if (incomplete(m_map)) {
        return QString();
    }
    return version().isEmpty() ? i18nc("@info version", "Unknown") : version();
}

QDateTime SlotInfo::buildTime() const
{
    const qint64 t = m_map.value(u"buildTime"_s).toLongLong();
    return t > 0 ? QDateTime::fromSecsSinceEpoch(t) : QDateTime();
}

QString SlotInfo::buildTimeText() const
{
    const QDateTime t = buildTime();
    if (!t.isValid()) {
        return i18nc("@info build date", "Unknown");
    }
    return QLocale().toString(t.toLocalTime(), QLocale::LongFormat);
}

QString SlotInfo::treeCommit() const
{
    return m_map.value(u"treeCommit"_s).toString();
}

QString SlotInfo::shortCommit() const
{
    const QString c = treeCommit();
    return c.isEmpty() ? i18nc("@info tree commit", "Unknown") : c.left(12);
}

QString SlotInfo::kernel() const
{
    return m_map.value(u"kernel"_s).toString();
}

bool SlotInfo::isEncrypted() const
{
    return m_map.value(u"encrypted"_s).toBool();
}

bool SlotInfo::isRunning() const
{
    return m_map.value(u"running"_s).toBool();
}

bool SlotInfo::isConfirmed() const
{
    return m_map.value(u"confirmed"_s).toBool();
}

bool SlotInfo::isTrial() const
{
    return m_map.value(u"trial"_s).toBool();
}

bool SlotInfo::isFailed() const
{
    return m_map.value(u"failed"_s).toBool();
}

bool SlotInfo::isBootable() const
{
    return !incomplete(m_map) && m_map.value(u"bootable"_s).toBool();
}

QString SlotInfo::reason() const
{
    const QString r = m_map.value(u"reason"_s).toString();
    if (r.isEmpty() && incomplete(m_map)) {
        return i18nc("@info", "An update stopped while writing this slot. The next update writes it again.");
    }
    return r;
}

QString SlotInfo::state() const
{
    if (incomplete(m_map)) {
        return u"incomplete"_s;
    }
    const QString s = m_map.value(u"state"_s).toString();
    if (!s.isEmpty()) {
        return s;
    }
    if (isTrial()) {
        return u"trial"_s;
    }
    if (isFailed()) {
        return u"failed"_s;
    }
    if (isConfirmed()) {
        return u"confirmed"_s;
    }
    return isPresent() ? u"installed"_s : u"empty"_s;
}

QString SlotInfo::stateText() const
{
    const QString s = state();
    QString text;
    if (s == "trial"_L1) {
        text = isRunning() ? i18nc("@info slot state", "On trial: kept once this session is confirmed") : i18nc("@info slot state", "Starts once at the next restart");
    } else if (s == "incomplete"_L1) {
        text = i18nc("@info slot state", "Incomplete");
    } else if (s == "failed"_L1) {
        text = i18nc("@info slot state", "Did not start: the computer went back to the other version");
    } else if (s == "confirmed"_L1) {
        text = i18nc("@info slot state", "Confirmed: the version this computer starts");
    } else if (s == "installed"_L1) {
        text = i18nc("@info slot state", "Installed, not in use");
    } else {
        text = i18nc("@info slot state", "Empty");
    }
    if (isRunning() && s != "trial"_L1) {
        return i18nc("@info %1 slot state", "Running now · %1", text);
    }
    return text;
}

}

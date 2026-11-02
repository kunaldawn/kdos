/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   eventstream — `kinstall --events json`, read into the Install page
 */

#include "eventstream.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Kdos
{

namespace
{
/* The log is for reading on the Done page and is bounded: a copy of a few
 * gigabytes logs a line per file it could not read, and the page must not
 * hold a list the size of the medium. The full log is /var/log/kinstall.log. */
constexpr int MaxLogLines = 2000;
}

EventStream::EventStream(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EventStream::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_steps.size());
}

QVariant EventStream::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Step &s = m_steps.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return s.title;
    case DetailRole:
        return s.detail;
    case StateRole:
        return s.state;
    case FracRole:
        return s.frac;
    case NoteRole:
        return s.note;
    default:
        return {};
    }
}

QHash<int, QByteArray> EventStream::roleNames() const
{
    return {
        {TitleRole, "title"},
        {DetailRole, "detail"},
        {StateRole, "stepState"},
        {FracRole, "frac"},
        {NoteRole, "note"},
    };
}

void EventStream::reset()
{
    beginResetModel();
    m_steps.clear();
    endResetModel();
    m_pending.clear();
    m_current = -1;
    m_note.clear();
    m_warnings.clear();
    m_log.clear();
    m_failure.clear();
    m_status = -1;
    Q_EMIT progressChanged();
    Q_EMIT warningsChanged();
    Q_EMIT logChanged();
}

QString EventStream::currentTitle() const
{
    return m_current >= 0 && m_current < m_steps.size() ? m_steps.at(m_current).title : QString();
}

double EventStream::overall() const
{
    int counted = 0;
    double done = 0;
    for (const Step &s : m_steps) {
        if (s.state == QLatin1String("skipped")) {
            continue;
        }
        counted++;
        if (s.state == QLatin1String("done")) {
            done += 1;
        } else if (s.state == QLatin1String("running") && s.frac > 0) {
            done += s.frac;
        }
    }
    return counted ? done / counted : 0;
}

void EventStream::touch(int step)
{
    if (step >= 0 && step < m_steps.size()) {
        const QModelIndex i = index(step);
        Q_EMIT dataChanged(i, i);
    }
}

void EventStream::setState(int step, const QString &state)
{
    if (step < 0 || step >= m_steps.size()) {
        return;
    }
    m_steps[step].state = state;
    touch(step);
}

bool EventStream::feed(const QByteArray &bytes)
{
    bool ok = true;
    m_pending += bytes;
    for (;;) {
        const qsizetype nl = m_pending.indexOf('\n');
        if (nl < 0) {
            break;
        }
        const QByteArray l = m_pending.left(nl).trimmed();
        m_pending.remove(0, nl + 1);
        if (!l.isEmpty() && !line(l)) {
            ok = false;
        }
    }
    return ok;
}

/*
 * ONE EVENT. A step begun closes every earlier running step, as the terminal
 * wizard's table does: a skipped step often sits between two, and "the one
 * before" would leave the real predecessor spinning.
 */
bool EventStream::line(const QByteArray &l)
{
    QJsonParseError err;
    const QJsonObject o = QJsonDocument::fromJson(l, &err).object();
    if (err.error != QJsonParseError::NoError || !o.contains(QStringLiteral("t"))) {
        return false;
    }
    const QString t = o.value(QStringLiteral("t")).toString();
    const int step = o.value(QStringLiteral("step")).toInt(-1);

    if (t == QLatin1String("plan")) {
        beginResetModel();
        m_steps.clear();
        const QJsonArray steps = o.value(QStringLiteral("steps")).toArray();
        for (const QJsonValue &v : steps) {
            const QJsonObject s = v.toObject();
            m_steps.append({s.value(QStringLiteral("title")).toString(), s.value(QStringLiteral("detail")).toString(),
                            s.value(QStringLiteral("state")).toString(), -1, QString()});
        }
        endResetModel();
    } else if (t == QLatin1String("S")) {
        for (int k = 0; k < step && k < m_steps.size(); k++) {
            if (m_steps.at(k).state == QLatin1String("running")) {
                setState(k, QStringLiteral("done"));
            }
        }
        m_current = step;
        m_note.clear();
        if (step >= 0 && step < m_steps.size()) {
            m_steps[step].frac = -1;
        }
        setState(step, QStringLiteral("running"));
    } else if (t == QLatin1String("K")) {
        setState(step, QStringLiteral("skipped"));
    } else if (t == QLatin1String("P")) {
        if (step >= 0 && step < m_steps.size()) {
            m_steps[step].frac = o.value(QStringLiteral("frac")).toDouble(-1);
            touch(step);
        }
    } else if (t == QLatin1String("N")) {
        m_note = o.value(QStringLiteral("text")).toString();
        if (step >= 0 && step < m_steps.size()) {
            m_steps[step].note = m_note;
            touch(step);
        }
    } else if (t == QLatin1String("L")) {
        m_log.append(o.value(QStringLiteral("text")).toString());
        if (m_log.size() > MaxLogLines) {
            m_log.remove(0, m_log.size() - MaxLogLines);
        }
        Q_EMIT logChanged();
        return true;
    } else if (t == QLatin1String("W")) {
        const QString w = o.value(QStringLiteral("text")).toString();
        if (!m_warnings.contains(w)) {
            m_warnings.append(w);
            Q_EMIT warningsChanged();
        }
        return true;
    } else if (t == QLatin1String("F")) {
        m_failure = o.value(QStringLiteral("text")).toString();
        setState(step, QStringLiteral("failed"));
    } else if (t == QLatin1String("D")) {
        for (int k = 0; k < m_steps.size(); k++) {
            if (m_steps.at(k).state == QLatin1String("running")) {
                setState(k, QStringLiteral("done"));
            }
        }
    } else if (t == QLatin1String("exit")) {
        m_status = o.value(QStringLiteral("status")).toInt(1);
        if (m_status != 0 && m_failure.isEmpty()) {
            m_failure = i18n("The install stopped without saying why.");
        }
        Q_EMIT progressChanged();
        Q_EMIT finished();
        return true;
    } else {
        return false;
    }
    Q_EMIT progressChanged();
    return true;
}

/*
 * THE PIPE CLOSED. With an exit line already read this is the expected end.
 * Without one the backend never finished: pkexec refused (126: not
 * authorised, 127: kinstall missing) or the process was killed, and the page
 * must say so rather than wait for ever.
 */
void EventStream::close(int exitCode)
{
    if (!m_pending.trimmed().isEmpty()) {
        line(m_pending.trimmed());
        m_pending.clear();
    }
    if (m_status >= 0) {
        return;
    }
    m_status = exitCode ? exitCode : 1;
    if (m_failure.isEmpty()) {
        if (exitCode == 126) {
            m_failure = i18n("Permission to install was not given.");
        } else if (exitCode == 127) {
            m_failure = i18n("The installer's backend, kinstall, could not be started.");
        } else {
            m_failure = i18n("The installer's backend stopped unexpectedly (status %1).", exitCode);
        }
    }
    if (m_current >= 0) {
        setState(m_current, QStringLiteral("failed"));
    }
    Q_EMIT progressChanged();
    Q_EMIT finished();
}

}

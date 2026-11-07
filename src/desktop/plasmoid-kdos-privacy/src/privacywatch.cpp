/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacywatch — `kdos privacy --watch --json`, kept running
 */

#include "privacywatch.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QStandardPaths>

namespace Kdos
{

namespace
{
/* A helper that ran this long before exiting was working; it is started again
 * at once rather than after the back-off it had built up. */
constexpr qint64 HealthyRunMs = 30 * 1000;
constexpr int FirstBackoffMs = 1000;
constexpr int MaxBackoffMs = 30 * 1000;
/* One state line is a few hundred bytes per capture. A helper that sends
 * this much without a newline is not sending state. */
constexpr qsizetype MaxLine = 1024 * 1024;
/* `kdos` answers 2 to a verb it does not have. */
constexpr int UsageExit = 2;
} // namespace

PrivacyWatch::PrivacyWatch(QObject *parent)
    : QObject(parent)
{
    m_restart.setSingleShot(true);
    connect(&m_restart, &QTimer::timeout, this, &PrivacyWatch::launch);
}

PrivacyWatch::~PrivacyWatch()
{
    m_stopping = true;
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString PrivacyWatch::program()
{
    QString path = QStandardPaths::findExecutable(QStringLiteral("kdos"));
    if (path.isEmpty()) {
        path = QStandardPaths::findExecutable(QStringLiteral("kdos"), {QStringLiteral("/usr/local/bin")});
    }
    return path;
}

void PrivacyWatch::start()
{
    if (!m_process && !m_restart.isActive()) {
        launch();
    }
}

bool PrivacyWatch::run(const QStringList &arguments)
{
    const QString kdos = program();
    return !kdos.isEmpty() && QProcess::startDetached(kdos, arguments);
}

void PrivacyWatch::launch()
{
    const QString kdos = program();
    if (kdos.isEmpty()) {
        setState({}, false, i18ndc("kdos-plasmoid-privacy", "@info", "The kdos command is not installed."));
        return;
    }
    m_buffer.clear();
    m_errors.clear();
    m_process = new QProcess(this);
    m_process->setProgram(kdos);
    m_process->setArguments({QStringLiteral("privacy"), QStringLiteral("--watch"), QStringLiteral("--json")});
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::readyReadStandardOutput, this, &PrivacyWatch::readLines);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_errors += m_process->readAllStandardError();
        if (m_errors.size() > 4096) {
            m_errors = m_errors.right(4096);
        }
    });
    connect(m_process, &QProcess::finished, this, &PrivacyWatch::finished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            finished(-1, QProcess::CrashExit);
        }
    });
    m_startedMs = QDateTime::currentMSecsSinceEpoch();
    m_process->start();
}

void PrivacyWatch::readLines()
{
    m_buffer += m_process->readAllStandardOutput();
    qsizetype nl;
    QByteArray last;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        last = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
    }
    if (m_buffer.size() > MaxLine) {
        m_buffer.clear();
    }
    if (last.isEmpty()) {
        return;
    }
    /* Only the newest complete line matters: each one is the whole state. */
    const PrivacyState s = kdosParsePrivacy(last);
    if (s.valid) {
        m_backoffMs = 0;
        setState(s, true, QString());
    }
}

void PrivacyWatch::finished(int code, QProcess::ExitStatus status)
{
    if (!m_process || m_stopping) {
        return;
    }
    const QString lastError = QString::fromLocal8Bit(m_errors).trimmed().section(QLatin1Char('\n'), -1);
    m_process->deleteLater();
    m_process = nullptr;

    if (status == QProcess::NormalExit && code == UsageExit) {
        setState({}, false, lastError.isEmpty() ? i18ndc("kdos-plasmoid-privacy", "@info", "This kdos has no privacy command.") : lastError);
        return;
    }
    setState({}, false, lastError.isEmpty() ? i18ndc("kdos-plasmoid-privacy", "@info", "The privacy helper stopped; starting it again.") : lastError);

    if (QDateTime::currentMSecsSinceEpoch() - m_startedMs >= HealthyRunMs) {
        m_backoffMs = 0;
        m_restart.start(0);
        return;
    }
    m_backoffMs = m_backoffMs ? qMin(m_backoffMs * 2, MaxBackoffMs) : FirstBackoffMs;
    m_restart.start(m_backoffMs);
}

void PrivacyWatch::setState(const PrivacyState &state, bool available, const QString &problem)
{
    m_state = state;
    m_available = available;
    m_problem = problem;
    Q_EMIT changed();
}

} // namespace Kdos

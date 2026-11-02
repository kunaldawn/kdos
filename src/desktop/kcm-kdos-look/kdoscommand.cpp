/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdoscommand — the `kdos` command, run as an argument vector
 */

#include "kdoscommand.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Kdos
{

namespace
{
/* An accent switch is a second of work; a minute is a command that hangs. */
constexpr int CommandTimeoutMs = 60 * 1000;

QString findKdos()
{
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("kdos"));
    if (!onPath.isEmpty()) {
        return onPath;
    }
    /* kdos-tools installs its links in /usr/local/bin, which a session
     * started without a login shell may not have on PATH. */
    const QString local = QStringLiteral("/usr/local/bin/kdos");
    return QFileInfo(local).isExecutable() ? local : QString();
}
}

KdosCommand::KdosCommand(const QString &program, QObject *parent)
    : QObject(parent)
    , m_program(program.isEmpty() ? findKdos() : program)
{
}

/* A page closed mid-command leaves the command to finish on its own: an
 * accent switch half done is worse than one done while nobody watches. */
KdosCommand::~KdosCommand()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->setParent(nullptr);
        connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);
        m_process = nullptr;
    }
}

QString KdosCommand::program() const
{
    return m_program;
}

bool KdosCommand::busy() const
{
    return m_process != nullptr || !m_queue.isEmpty();
}

void KdosCommand::run(const QStringList &arguments)
{
    const bool wasBusy = busy();
    m_queue.enqueue(arguments);
    if (!wasBusy) {
        Q_EMIT busyChanged();
    }
    startNext();
}

/*
 * A HANDLER OF finished() MAY CALL run(), and that run starts the next
 * command itself; so this starts one only when none is running, and every
 * caller may call it. Two commands running at once would be the very thing
 * the queue exists to prevent.
 */
void KdosCommand::startNext()
{
    if (m_process) {
        return;
    }
    if (m_queue.isEmpty()) {
        Q_EMIT busyChanged();
        return;
    }
    const QStringList arguments = m_queue.dequeue();
    if (m_program.isEmpty()) {
        Result r;
        r.arguments = arguments;
        Q_EMIT finished(r);
        startNext();
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(m_program);
    m_process->setArguments(arguments);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    auto *process = m_process;
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, &QProcess::kill);

    connect(process, &QProcess::finished, this, [this, process, arguments](int exitCode, QProcess::ExitStatus status) {
        Result r;
        r.arguments = arguments;
        r.started = true;
        r.exitCode = status == QProcess::NormalExit ? exitCode : -1;
        r.out = QString::fromUtf8(process->readAllStandardOutput());
        r.err = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        m_process = nullptr;
        Q_EMIT finished(r);
        startNext();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, arguments](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        Result r;
        r.arguments = arguments;
        r.err = process->errorString();
        process->deleteLater();
        m_process = nullptr;
        Q_EMIT finished(r);
        startNext();
    });

    process->start();
    timer->start(CommandTimeoutMs);
}

} // namespace Kdos

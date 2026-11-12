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

#include "boxtool.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QVariant>

#include <memory>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
/* How long a quitting System Settings waits for an act still running.
 * Longer than any create or stop takes; an export of a large application
 * may still be cut short, and kdos-appbox writes its archive whole or not at
 * all. */
constexpr int ActGraceMs = 120 * 1000;
const char *const KindProperty = "kdos-box-act";
}

BoxTool::BoxTool(QObject *parent)
    : QObject(parent)
{
}

BoxTool::~BoxTool()
{
    const auto procs = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *proc : procs) {
        /* The callbacks belong to a page that is going away. */
        disconnect(proc, nullptr, this, nullptr);
        if (proc->state() == QProcess::NotRunning || !proc->property(KindProperty).toBool() || !QCoreApplication::instance()) {
            continue;
        }
        /* An act outlives the page without holding the window: it is handed
         * to the application, finishes on its own and deletes itself. Only
         * an application that quits under it waits, with its window already
         * gone. A query is a child still, and QProcess's destructor kills
         * it. */
        QCoreApplication *app = QCoreApplication::instance();
        proc->setParent(app);
        connect(proc, &QProcess::finished, proc, &QObject::deleteLater);
        connect(app, &QCoreApplication::aboutToQuit, proc, [proc] {
            proc->waitForFinished(ActGraceMs);
        });
    }
}

bool BoxTool::isValidBoxName(const QString &name)
{
    if (name.isEmpty() || name.toUtf8().size() >= 64) {
        return false;
    }
    if (name.startsWith(u'.') || name.startsWith(u'-')) {
        return false;
    }
    for (const QChar c : name) {
        const char16_t u = c.unicode();
        const bool ok = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') || u == u'.' || u == u'_' || u == u'-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool BoxTool::isBusy() const
{
    return m_running > 0;
}

void BoxTool::run(const QString &program, const QStringList &args, Callback done, Kind kind)
{
    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setProperty(KindProperty, kind == Kind::Act);

    /* Resolved per call against the $PATH of this process, so the lookup
     * sees what a prompt in the same session would. */
    const QString exe = QStandardPaths::findExecutable(program);
    if (exe.isEmpty()) {
        Result r;
        r.output = i18nc("@info %1 program name", "%1 is not installed.", program);
        r.lastLine = r.output;
        proc->deleteLater();
        done(r);
        return;
    }
    proc->setProgram(exe);
    proc->setArguments(args);

    auto buffer = std::make_shared<QString>();
    /* Only an act reports progress: a query that runs beside it (the
     * listing on its timer) would otherwise put its JSON in the act's
     * progress line. */
    const bool act = kind == Kind::Act;
    connect(proc, &QProcess::readyRead, this, [this, proc, buffer, act] {
        const QString chunk = QString::fromUtf8(proc->readAll());
        buffer->append(chunk);
        const auto lines = chunk.split(u'\n', Qt::SkipEmptyParts);
        if (act && !lines.isEmpty()) {
            Q_EMIT progress(lines.last().trimmed());
        }
    });

    ++m_running;
    if (m_running == 1) {
        Q_EMIT busyChanged();
    }
    connect(proc, &QProcess::finished, this, [this, proc, buffer, done](int code, QProcess::ExitStatus status) {
        buffer->append(QString::fromUtf8(proc->readAll()));
        Result r;
        r.started = true;
        r.exitCode = status == QProcess::NormalExit ? code : -1;
        r.output = *buffer;
        const auto lines = buffer->split(u'\n', Qt::SkipEmptyParts);
        for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
            if (!it->trimmed().isEmpty()) {
                r.lastLine = it->trimmed();
                break;
            }
        }
        proc->deleteLater();
        --m_running;
        if (m_running == 0) {
            Q_EMIT busyChanged();
        }
        done(r);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, done, program](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        Result r;
        r.output = i18nc("@info %1 program name", "%1 could not be started.", program);
        r.lastLine = r.output;
        proc->deleteLater();
        --m_running;
        if (m_running == 0) {
            Q_EMIT busyChanged();
        }
        done(r);
    });
    proc->start();
}

void BoxTool::runSequence(const QList<std::pair<QString, QStringList>> &steps, Callback done)
{
    if (steps.isEmpty()) {
        Result r;
        r.started = true;
        r.exitCode = 0;
        done(r);
        return;
    }
    runFrom(steps, 0, std::move(done));
}

void BoxTool::runFrom(const QList<std::pair<QString, QStringList>> &steps, int index, Callback done)
{
    const auto &step = steps.at(index);
    run(
        step.first,
        step.second,
        [this, steps, index, done](const Result &result) {
            Result r = result;
            r.step = index;
            if (r.exitCode != 0 || index + 1 >= steps.size()) {
                done(r);
                return;
            }
            runFrom(steps, index + 1, done);
        },
        Kind::Act);
}

}

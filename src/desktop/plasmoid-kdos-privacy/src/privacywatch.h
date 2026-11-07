/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   privacywatch — `kdos privacy --watch --json`, kept running
 *
 * The helper prints a line at once and another each time a capture starts,
 * stops or is muted, so the lamps follow it with no polling here. A helper
 * that exits is started again, waiting longer each time it fails quickly; one
 * that exits 2 does not know the verb, and is not started again.
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

#include "privacystate.h"

namespace Kdos
{

class PrivacyWatch : public QObject
{
    Q_OBJECT

public:
    explicit PrivacyWatch(QObject *parent = nullptr);
    ~PrivacyWatch() override;

    void start();

    const PrivacyState &state() const
    {
        return m_state;
    }
    bool available() const
    {
        return m_available;
    }
    QString problem() const
    {
        return m_problem;
    }

    /* `kdos <arguments>`, detached; false when kdos cannot be started. */
    bool run(const QStringList &arguments);

    /* Where kdos is: the session's PATH, then /usr/local/bin, where
     * kdos-tools installs it. Empty when it is in neither. */
    static QString program();

Q_SIGNALS:
    void changed();

private:
    void launch();
    void readLines();
    void finished(int code, QProcess::ExitStatus status);
    void setState(const PrivacyState &state, bool available, const QString &problem);

    QProcess *m_process = nullptr;
    QTimer m_restart;
    QByteArray m_buffer;
    QByteArray m_errors;
    PrivacyState m_state;
    bool m_available = false;
    bool m_stopping = false;
    QString m_problem;
    int m_backoffMs = 0;
    qint64 m_startedMs = 0;
};

} // namespace Kdos

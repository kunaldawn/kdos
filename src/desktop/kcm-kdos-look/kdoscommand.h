/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdoscommand — the `kdos` command, run as an argument vector
 *
 * EVERY ACTION ON THE PAGE IS A `kdos` VERB. `kdos theme <accent>` and
 * `kdos phosphor on|off` are the one implementation of what they switch;
 * the welcome tour and KRunner run the same verbs, so the page cannot do
 * something the command line does not. The program and its arguments go to
 * QProcess as a list, never through a shell, and an accent name reaches it
 * only after the page has found it in `kdos theme list`.
 *
 * ONE COMMAND AT A TIME, IN ORDER. Two theme runs at once would write the
 * same files in an order nobody chose, so a request made while one runs
 * waits behind it.
 */

#pragma once

#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

class QProcess;

namespace Kdos
{

class KdosCommand : public QObject
{
    Q_OBJECT

public:
    struct Result {
        QStringList arguments;
        int exitCode = -1;
        bool started = false;
        QString out;
        QString err;
    };

    /* `program` empty is `kdos` from PATH, then /usr/local/bin/kdos. */
    explicit KdosCommand(const QString &program = QString(), QObject *parent = nullptr);
    ~KdosCommand() override;

    QString program() const;
    bool busy() const;

    void run(const QStringList &arguments);

Q_SIGNALS:
    void busyChanged();
    void finished(const Kdos::KdosCommand::Result &result);

private:
    void startNext();

    QString m_program;
    QQueue<QStringList> m_queue;
    QProcess *m_process = nullptr;
};

} // namespace Kdos

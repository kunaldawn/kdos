/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   eventstream — `kinstall --events json`, read into the Install page
 *
 * One JSON object per line, as kinstall-core's events.c documents: the plan,
 * the step events, the exit. The steps become a list model the page draws;
 * the log and the warnings are kept for the Done page. Bytes arrive as the
 * pipe delivers them, so a line is only read once its newline has come.
 */

#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace Kdos
{

class EventStream : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The event stream belongs to the Backend")

    Q_PROPERTY(int currentStep READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY progressChanged)
    Q_PROPERTY(QString note READ note NOTIFY progressChanged)
    /* The whole install's progress, 0..1: finished steps, plus the running
     * step's own fraction where it has one. */
    Q_PROPERTY(double overall READ overall NOTIFY progressChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY warningsChanged)
    Q_PROPERTY(QStringList log READ log NOTIFY logChanged)
    Q_PROPERTY(QString failure READ failure NOTIFY finished)
    /* -1 while running, else the exit line's status. */
    Q_PROPERTY(int status READ status NOTIFY finished)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        DetailRole,
        StateRole,
        FracRole,
        NoteRole,
    };
    Q_ENUM(Roles)

    explicit EventStream(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /* Bytes off the pipe. Returns false once a line could not be read as an
     * event, which is a backend that is not the one this front end knows. */
    bool feed(const QByteArray &bytes);
    /* The pipe closed. A stream that ended without its exit line is a
     * backend that died, and is reported as a failure. */
    void close(int exitCode);
    void reset();

    int currentStep() const { return m_current; }
    QString currentTitle() const;
    QString note() const { return m_note; }
    double overall() const;
    QStringList warnings() const { return m_warnings; }
    QStringList log() const { return m_log; }
    QString failure() const { return m_failure; }
    int status() const { return m_status; }

Q_SIGNALS:
    void progressChanged();
    void warningsChanged();
    void logChanged();
    void finished();

private:
    struct Step {
        QString title, detail, state;
        double frac = -1;
        QString note;
    };
    bool line(const QByteArray &l);
    void setState(int step, const QString &state);
    void touch(int step);

    QList<Step> m_steps;
    QByteArray m_pending;
    int m_current = -1;
    QString m_note;
    QStringList m_warnings;
    QStringList m_log;
    QString m_failure;
    int m_status = -1;
};

}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   healthcontroller — run `kdos doctor --json`, and hold what it found
 *
 * kinfocenter's own pages run a command through CommandOutputContext and
 * show its text. That class is kinfocenter's private library, with no header
 * installed and a note that it may not be linked to, and plain text cannot
 * mark a failed check as one. So this keeps the same shape — a program and
 * its arguments, ready, error and explanation, a filter, refresh — and reads
 * doctor's JSON records instead, one row per check.
 *
 * EXIT 0 AND 1 ARE BOTH A REPORT: doctor exits 1 when any check warned, and
 * that is the report the page exists to show. Anything else, a report that
 * does not parse, or a program that cannot be started is an error, said with
 * what doctor wrote to stderr.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "doctormodel.h"

class QProcess;

namespace Kdos
{

class HealthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *checks READ checks CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString error READ error NOTIFY resultChanged)
    Q_PROPERTY(QString explanation READ explanation NOTIFY resultChanged)
    Q_PROPERTY(int warnings READ warnings NOTIFY resultChanged)
    Q_PROPERTY(int passed READ passed NOTIFY resultChanged)
    Q_PROPERTY(int skipped READ skipped NOTIFY resultChanged)
    Q_PROPERTY(int total READ total NOTIFY resultChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString text READ text NOTIFY resultChanged)

public:
    /* `program` empty is `kdos` from PATH, then /usr/local/bin/kdos. */
    explicit HealthController(const QString &program = QString(), QObject *parent = nullptr);
    ~HealthController() override;

    QAbstractItemModel *checks() const;
    DoctorModel *report() const;
    bool ready() const;
    QString error() const;
    QString explanation() const;
    int warnings() const;
    int passed() const;
    int skipped() const;
    int total() const;
    QString filter() const;
    void setFilter(const QString &filter);
    QString text() const;

    QString program() const;
    QStringList arguments() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copyToClipboard() const;

Q_SIGNALS:
    void readyChanged();
    void resultChanged();
    void filterChanged();

private:
    void finish(int exitCode, bool crashed, const QByteArray &out, const QByteArray &err);
    void setError(const QString &error, const QString &explanation);

    QString m_program;
    DoctorModel *m_model;
    DoctorFilterModel *m_filtered;
    QProcess *m_process = nullptr;
    bool m_ready = false;
    bool m_again = false;
    QString m_error;
    QString m_explanation;
    QString m_filter;
};

} // namespace Kdos

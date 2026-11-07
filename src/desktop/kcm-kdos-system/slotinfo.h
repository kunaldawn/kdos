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
 * One root slot as the page shows it: one ListSlots dictionary, typed.
 *
 * A KEY THE SERVER LEAVES OUT READS AS UNKNOWN, NEVER AS A GUESS. An empty
 * version is shown as "unknown" and a zero build time as no date, because a
 * slot whose root could not be read is still a slot the machine can start,
 * and a made-up version beside a "Start this version" button is the one
 * place a wrong word sends a person into the wrong system.
 */

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Kdos
{

class SlotInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString slotId READ slotId NOTIFY changed)
    Q_PROPERTY(QString letter READ letter NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool present READ isPresent NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString versionText READ versionText NOTIFY changed)
    Q_PROPERTY(QDateTime buildTime READ buildTime NOTIFY changed)
    Q_PROPERTY(QString buildTimeText READ buildTimeText NOTIFY changed)
    Q_PROPERTY(QString treeCommit READ treeCommit NOTIFY changed)
    Q_PROPERTY(QString shortCommit READ shortCommit NOTIFY changed)
    Q_PROPERTY(QString kernel READ kernel NOTIFY changed)
    Q_PROPERTY(bool encrypted READ isEncrypted NOTIFY changed)
    Q_PROPERTY(bool running READ isRunning NOTIFY changed)
    Q_PROPERTY(bool confirmed READ isConfirmed NOTIFY changed)
    Q_PROPERTY(bool trial READ isTrial NOTIFY changed)
    Q_PROPERTY(bool failed READ isFailed NOTIFY changed)
    Q_PROPERTY(bool bootable READ isBootable NOTIFY changed)
    Q_PROPERTY(QString reason READ reason NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)

public:
    explicit SlotInfo(const QString &slotId, QObject *parent = nullptr);

    /* Replaces every field; a key missing from `map` resets its field. */
    void update(const QVariantMap &map);
    void clear();

    QString slotId() const;
    QString letter() const;
    QString title() const;
    bool isPresent() const;
    QString version() const;
    QString versionText() const;
    QDateTime buildTime() const;
    QString buildTimeText() const;
    QString treeCommit() const;
    QString shortCommit() const;
    QString kernel() const;
    bool isEncrypted() const;
    bool isRunning() const;
    bool isConfirmed() const;
    bool isTrial() const;
    bool isFailed() const;
    bool isBootable() const;
    QString reason() const;
    QString state() const;
    QString stateText() const;

Q_SIGNALS:
    void changed();

private:
    QString m_id;
    QVariantMap m_map;
};

}

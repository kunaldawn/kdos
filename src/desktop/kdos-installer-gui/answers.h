/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   answers — what the pages collect, as kinstall's answer file
 *
 * Every page writes here and nothing else; the answer file is the one thing
 * handed to kinstall, on its standard input, for `--dump plan` and for the
 * install alike. So the summary shows exactly what will run: the backend
 * read the same text both times.
 */

#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace Kdos
{

class Answers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Answers belong to the Backend")

    Q_PROPERTY(QString timezone MEMBER m_timezone NOTIFY changed)
    Q_PROPERTY(QString xkbLayout MEMBER m_xkbLayout NOTIFY changed)
    Q_PROPERTY(QString xkbVariant MEMBER m_xkbVariant NOTIFY changed)

    Q_PROPERTY(QString disk MEMBER m_disk NOTIFY changed)
    /* wipe or reuse; Partition Manager's layouts are installed as reuse. */
    Q_PROPERTY(QString plan MEMBER m_plan NOTIFY changed)
    Q_PROPERTY(QString esp MEMBER m_esp NOTIFY changed)
    Q_PROPERTY(QString root MEMBER m_root NOTIFY changed)
    Q_PROPERTY(bool formatEsp MEMBER m_formatEsp NOTIFY changed)
    Q_PROPERTY(QString fstype MEMBER m_fstype NOTIFY changed)
    /* none, file or partition. */
    Q_PROPERTY(QString swap MEMBER m_swap NOTIFY changed)
    Q_PROPERTY(int swapMb MEMBER m_swapMb NOTIFY changed)
    Q_PROPERTY(bool lvm MEMBER m_lvm NOTIFY changed)
    Q_PROPERTY(bool luks MEMBER m_luks NOTIFY changed)
    Q_PROPERTY(QString luksPassphrase MEMBER m_luksPassphrase NOTIFY changed)

    Q_PROPERTY(QString hostname MEMBER m_hostname NOTIFY changed)
    Q_PROPERTY(bool oem MEMBER m_oem NOTIFY changed)
    Q_PROPERTY(QString fullname MEMBER m_fullname NOTIFY changed)
    Q_PROPERTY(QString username MEMBER m_username NOTIFY changed)
    Q_PROPERTY(QString password MEMBER m_password NOTIFY changed)
    Q_PROPERTY(bool admin MEMBER m_admin NOTIFY changed)
    Q_PROPERTY(bool rootLocked MEMBER m_rootLocked NOTIFY changed)
    Q_PROPERTY(QString rootPassword MEMBER m_rootPassword NOTIFY changed)
    Q_PROPERTY(bool autologin MEMBER m_autologin NOTIFY changed)
    Q_PROPERTY(bool autologinLock MEMBER m_autologinLock NOTIFY changed)

    Q_PROPERTY(QString theme MEMBER m_theme NOTIFY changed)
    Q_PROPERTY(QString desk MEMBER m_desk NOTIFY changed)
    Q_PROPERTY(bool alienApps MEMBER m_alienApps NOTIFY changed)
    Q_PROPERTY(QStringList apps MEMBER m_apps NOTIFY changed)

public:
    explicit Answers(QObject *parent = nullptr);

    /* The answer file. Secrets are in it: it is only ever written to a
     * pipe, never to disk. */
    QString toText() const;

    /* A value an answer file can carry: no line break, and no space at
     * either end, which the backend's parser trims off. */
    Q_INVOKABLE static bool fitsAnswerFile(const QString &value);
    /* The user name the backend accepts for a full name, as a suggestion. */
    Q_INVOKABLE static QString suggestUsername(const QString &fullname);
    Q_INVOKABLE static bool usernameOk(const QString &name);
    Q_INVOKABLE static bool hostnameOk(const QString &name);
    /* conf.c's ki_fullname_ok: passwd's GECOS field holds no ':' and no
     * control character. */
    Q_INVOKABLE static bool fullnameOk(const QString &name);

    Q_INVOKABLE void setAppChosen(const QString &group, bool chosen);

    QString disk() const { return m_disk; }
    bool oem() const { return m_oem; }

Q_SIGNALS:
    void changed();

private:
    QString m_timezone = QStringLiteral("UTC");
    QString m_xkbLayout = QStringLiteral("us");
    QString m_xkbVariant;
    QString m_disk;
    QString m_plan = QStringLiteral("wipe");
    QString m_esp;
    QString m_root;
    bool m_formatEsp = true;
    QString m_fstype = QStringLiteral("ext4");
    QString m_swap = QStringLiteral("file");
    int m_swapMb = 4096;
    bool m_lvm = true;
    bool m_luks = false;
    QString m_luksPassphrase;
    QString m_hostname = QStringLiteral("kdos");
    bool m_oem = false;
    QString m_fullname;
    QString m_username;
    QString m_password;
    bool m_admin = true;
    bool m_rootLocked = true;
    QString m_rootPassword;
    bool m_autologin = false;
    bool m_autologinLock = true;
    QString m_theme = QStringLiteral("bone");
    QString m_desk = QStringLiteral("kdos");
    bool m_alienApps = true;
    QStringList m_apps;
};

}

/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — an Entry as Discover's AbstractResource
 *
 * The resource only presents. Every value comes from the Entry the backend
 * built from the tools' answers, and a refresh replaces the Entry whole; the
 * resource then says which of its properties moved.
 * ---------------------------------
 */

#pragma once

#include "KdosEntry.h"
#include "KdosParse.h"

#include <resources/AbstractResource.h>

#include <QVariantList>

namespace Kdos
{

class KdosBackend;

class KdosResource : public AbstractResource
{
    Q_OBJECT
    // Read by KdosNotice.qml and KdosBox.qml on the application page.
    Q_PROPERTY(QString kdosNotice READ kdosNotice NOTIFY entryChanged)
    Q_PROPERTY(int kdosNoticeType READ kdosNoticeType NOTIFY entryChanged)
    Q_PROPERTY(QString boxName READ boxName NOTIFY entryChanged)
    Q_PROPERTY(QString boxBase READ boxBase NOTIFY entryChanged)
    Q_PROPERTY(QString boxState READ boxState NOTIFY entryChanged)
    Q_PROPERTY(QString boxAccent READ boxAccent NOTIFY entryChanged)
    Q_PROPERTY(QString boxPersistence READ boxPersistence NOTIFY entryChanged)
    // The same four as a person reads them, for the page; the raw values
    // above are what `kdos-box list` printed.
    Q_PROPERTY(QString boxBaseText READ boxBaseText NOTIFY entryChanged)
    Q_PROPERTY(QString boxStateText READ boxStateText NOTIFY entryChanged)
    Q_PROPERTY(QString boxAccentText READ boxAccentText NOTIFY entryChanged)
    Q_PROPERTY(QString boxPersistenceText READ boxPersistenceText NOTIFY entryChanged)
    Q_PROPERTY(QStringList memberNames READ memberNames NOTIFY entryChanged)
    // What the box is allowed, read by fetchBoxGrants(): a list of
    // {label, value} maps, and what kdos-box says it cannot deliver.
    Q_PROPERTY(QVariantList boxGrants READ boxGrants NOTIFY boxGrantsChanged)
    Q_PROPERTY(QStringList boxWarnings READ boxWarnings NOTIFY boxGrantsChanged)

public:
    KdosResource(const Entry &entry, KdosBackend *backend);

    const Entry &entry() const
    {
        return m_entry;
    }
    // Replaces what the resource presents; emits the change signals for what
    // differs.
    void setEntry(const Entry &entry);
    // The resource's identity in the backend: lane and id.
    static QString keyOf(const Entry &entry);
    QString key() const
    {
        return keyOf(m_entry);
    }

    QString packageName() const override;
    QString name() const override;
    QString comment() override;
    QVariant icon() const override;
    bool canExecute() const override;
    void invokeApplication() const override;
    State state() override;
    bool hasCategory(const QString &category) const override;
    QUrl homepage() override;
    Type type() const override;
    quint64 size() override;
    QString sizeDescription() override;
    QJsonArray licenses() override;
    QString installedVersion() const override;
    QString availableVersion() const override;
    QString longDescription() override;
    QString origin() const override;
    QString displayOrigin() const override;
    QString section() override;
    QString author() const override;
    QStringList mimetypes() const override;
    QList<PackageState> addonsInformation() override;
    QString appstreamId() const override;
    QUrl url() const override;
    QString sourceIcon() const override;
    QDate releaseDate() const override;
    bool isRemovable() const override;
    QString executeLabel() const override;
    QStringList topObjects() const override;
    QStringList bottomObjects() const override;
    QString verifiedMessage() const override;
    QString verifiedIconName() const override;

    // What a search finds the resource by besides its name, package and
    // summary: the names and keywords of the other components of a host
    // port's package. Kept apart from the Entry, and set again at every
    // refresh that lists the port.
    void setSearchTerms(const QStringList &terms)
    {
        m_searchTerms = terms;
    }
    const QStringList &searchTerms() const
    {
        return m_searchTerms;
    }

    // The page's notice: why the last install or removal failed, while that
    // is set, else what the lane says before an install.
    QString kdosNotice() const;
    int kdosNoticeType() const;
    // "<name> was not installed: <why>", from the transaction that failed;
    // the next transaction of this resource clears it. It is kept apart from
    // the Entry, which every refresh replaces.
    void setFailure(const QString &said);
    QString boxName() const
    {
        return m_entry.box;
    }
    QString boxBase() const
    {
        return m_entry.boxBase;
    }
    QString boxState() const
    {
        return m_entry.boxState;
    }
    QString boxAccent() const
    {
        return m_entry.boxAccent;
    }
    QString boxPersistence() const
    {
        return m_entry.boxPersistence;
    }
    QString boxBaseText() const;
    QString boxStateText() const;
    QString boxAccentText() const;
    QString boxPersistenceText() const;
    QStringList memberNames() const;
    QVariantList boxGrants() const
    {
        return m_grants;
    }
    QStringList boxWarnings() const
    {
        return m_grantWarnings;
    }
    // Asks `kdos-box profile` for the box's permissions; the page calls it
    // when it shows the box, so a list of resources asks nothing.
    Q_INVOKABLE void fetchBoxGrants();
    // The backend's answer to fetchBoxGrants().
    void setBoxGrants(const BoxGrants &grants);

public Q_SLOTS:
    void fetchChangelog() override;
    void fetchScreenshots() override;

Q_SIGNALS:
    void entryChanged();
    void boxGrantsChanged();

private:
    KdosBackend *kdosBackend() const;

    Entry m_entry;
    QString m_failure;
    QStringList m_searchTerms;
    QVariantList m_grants;
    QStringList m_grantWarnings;
};

}

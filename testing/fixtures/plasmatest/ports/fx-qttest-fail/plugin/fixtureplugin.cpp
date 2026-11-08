#include <QObject>
#include <QtPlugin>

// Defined nowhere: the plugin links, and fails to load.
void kdos_fixture_missing();

namespace Kdos
{
class FixturePlugin : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kdos.plasmatest.fixture")
public:
    FixturePlugin()
    {
        kdos_fixture_missing();
    }
};
}

#include "fixtureplugin.moc"

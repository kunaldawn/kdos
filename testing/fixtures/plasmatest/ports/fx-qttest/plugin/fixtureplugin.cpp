#include <QObject>
#include <QtPlugin>

namespace Kdos
{
class FixturePlugin : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kdos.plasmatest.fixture")
};
}

#include "fixtureplugin.moc"

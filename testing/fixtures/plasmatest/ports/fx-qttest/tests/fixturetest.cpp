#include <QTest>

namespace Kdos
{
class FixtureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sums()
    {
        QCOMPARE(2 + 2, 4);
    }
};
}

QTEST_GUILESS_MAIN(Kdos::FixtureTest)
#include "fixturetest.moc"

#include <QTest>

namespace Kdos
{
class FixtureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sums()
    {
        QCOMPARE(2 + 2, 5);
    }
};
}

QTEST_GUILESS_MAIN(Kdos::FixtureTest)
#include "fixturetest.moc"

// The colour-literal check's fixture: every line ending in `// literal` sets
// a colour from a literal and must be reported; no other line may be.
#include <KColorScheme>
#include <QColor>
#include <QString>

namespace Kdos
{
QColor role()
{
    return KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::NormalText).color();
}
QColor fromInts() { return QColor(255, 0, 0); } // literal
QColor fromHex() { return QColor(QStringLiteral("#00ff00")); } // literal
QColor fromString() { return QColor("#0000ff"); } // literal
QColor fromGlobal() { return Qt::red; } // literal
QColor fromRgb() { return QColor::fromRgb(0x12, 0x34, 0x56); } // literal
QColor fromRgba() { return QColor(0x80123456); } // literal
QColor clear() { return QColor(Qt::transparent); }
QColor copy(const QColor &c) { return QColor(c); }
// QColor(1, 2, 3) in a comment is not code, nor is "#123456"
const char *page = "https://example.invalid/#abcdef";
const QString issue = QStringLiteral("issue #123456");
}

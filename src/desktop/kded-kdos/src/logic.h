/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the decisions, with no session under them
 *
 * Everything the module decides from a name, a file or a directory, as plain
 * functions over plain arguments, so the QtTest suite can feed them fixtures
 * and no test needs a bus, a colour scheme or a USB stick.
 * ---------------------------------
 */

#pragma once

#include <QString>
#include <QStringList>

namespace Kdos
{

/* The accent a colour scheme name is KDOS's for: "KDOSAmber" is "amber".
 * Anything that is not "KDOS" followed by one capitalised word — Breeze, a
 * user's "KDOSAmber edited", plain "KDOS", and KDOSBox, the neutral scheme
 * of a box's title bar — is not, and comes back empty. The
 * names are the generator's `KDOS<Title>`, so this never needs to know which
 * accents exist: `kdos theme` refuses a name the palette does not have. */
QString accentFromScheme(const QString &scheme);

/* Whether `scheme` is a scheme the system ships: a `color-schemes/<scheme>
 * .colors` under one of `dataDirs` other than the user's own `userDataDir`.
 * A scheme that lives only in the user's directory was made by the user,
 * whatever it is called, and its colours are the user's to keep. */
bool isShippedScheme(const QString &scheme, const QString &userDataDir,
                     const QStringList &dataDirs);

/* The first word of the accent file `kdos theme` writes, or empty when the
 * file is missing, unreadable or blank. */
QString readAccent(const QString &path);

/* The exported application sets directly inside `dir`: regular files whose
 * name ends in `.ktar`, lower case, not symlinks, not empty, readable, sorted
 * by name, at most `max` of them. The suffix is the one Discover's KDOS
 * backend and the installer accept, so a file offered here is a file Discover
 * opens; `SETS.KTAR` would be an offer that opens an empty page. The walk
 * stops after `scanLimit` directory entries, so a stick with a hundred
 * thousand files at its root costs a bounded read. */
QStringList ktarSets(const QString &dir, int max, int scanLimit);

/* Whether `dir` holds a Samba usershare: a regular file, not a symlink, whose
 * name begins with neither `.` nor `:`. `net usershare add` writes each share
 * to a `:tmpXXXXXX` file and renames it to the share's name, so a share still
 * being written is not one yet; Samba's service script applies the same
 * test. The walk stops after `scanLimit` directory entries. */
bool hasUsershare(const QString &dir, int scanLimit);

}

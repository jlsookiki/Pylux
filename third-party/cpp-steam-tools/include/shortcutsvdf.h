#ifndef SHORTCUTSVDF_H
#define SHORTCUTSVDF_H

#include "steamshortcutentry.h"

#include <QByteArray>
#include <QString>
#include <QVector>

// Steam's binary KeyValues as used by userdata/<id>/config/shortcuts.vdf.
//
//   00 "shortcuts" 00
//     00 "0" 00                      one object per shortcut, keyed by index
//       02 "appid" 00 <int32 LE>
//       01 "AppName" 00 "..." 00
//       00 "tags" 00                 nested object = list, children keyed "0","1",...
//         01 "0" 00 "favorite" 00
//       08
//     08
//   08
//   08
//
// parse() rejects anything it does not fully understand instead of
// returning a partial list, so a caller never rewrites the file from a
// truncated view of it. Fields the library does not know about are kept
// and written back unchanged.
namespace ShortcutsVdf {

bool parse(const QByteArray &data, QVector<SteamShortcutEntry> &out, QString *error);
QByteArray serialize(const QVector<SteamShortcutEntry> &shortcuts);

} // namespace ShortcutsVdf

#endif // SHORTCUTSVDF_H

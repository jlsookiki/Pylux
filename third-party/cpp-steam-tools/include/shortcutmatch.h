#ifndef SHORTCUTMATCH_H
#define SHORTCUTMATCH_H

#include "cpp-steam-tools_global.h"
#include "steamshortcutentry.h"

#include <QVector>

// Rules for re-adding a shortcut that may already be in the library.
namespace ShortcutMatch {

// Returns a fresh entry that keeps everything Steam or the user put on
// `existing` (LastPlayTime, favorite and collection tags, hidden/overlay
// flags, fields this library does not know) and takes only the fields the
// app owns from `fresh`: appid, AppName, Exe, StartDir, icon, LaunchOptions.
// Tags are the union, existing order first.
CPPSTEAMTOOLS_EXPORT SteamShortcutEntry merge(const SteamShortcutEntry &existing, const SteamShortcutEntry &fresh);

} // namespace ShortcutMatch

#endif // SHORTCUTMATCH_H

#ifndef SHORTCUTMATCH_H
#define SHORTCUTMATCH_H

#include "cpp-steam-tools_global.h"
#include "steamshortcutentry.h"

#include <QVector>

// Rules for re-adding a shortcut that may already be in the library.
namespace ShortcutMatch {

// Returns a merged entry that keeps everything Steam or the user put on
// `existing` (LastPlayTime, favorite and collection tags, hidden/overlay
// flags, fields this library does not know) and takes only the fields the
// app owns from `fresh`: appid, AppName, Exe, StartDir, icon, LaunchOptions.
// Tags are the union, existing order first.
CPPSTEAMTOOLS_EXPORT SteamShortcutEntry merge(const SteamShortcutEntry &existing, const SteamShortcutEntry &fresh);

// Index of the entry `fresh` should update, or -1 to append. Matches, in
// order: the same appid; the same Exe and LaunchOptions; the same name but
// only when the existing tile is one of ours (tagged "Remote Play", or
// started by the same program, where for Flatpaks the program is the app id
// after "run"), so a Heroic or Lutris tile sharing a game's name is left alone.
CPPSTEAMTOOLS_EXPORT int findExisting(const QVector<SteamShortcutEntry> &entries, const SteamShortcutEntry &fresh);

} // namespace ShortcutMatch

#endif // SHORTCUTMATCH_H

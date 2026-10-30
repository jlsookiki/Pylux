# cpp-steam-tools (vendored)

Writes non-Steam shortcuts and Steam Deck controller layouts for the signed-in Steam user.

Originally a git submodule of [streetpea/cpp-steam-tools](https://github.com/streetpea/cpp-steam-tools) (GPL-3.0, see `LICENSE`). Vendored here because the shortcut path needed changes the upstream state machine could not absorb:

- `shortcutsvdf.*` is a complete binary KeyValues reader/writer. It keeps every field it reads, including ones it does not know, and refuses to parse a file it does not fully understand. The previous parser matched `lastplaytime` against Steam's `LastPlayTime`, treated the field as a boolean, and so reset every non-Steam game's last-played time on each write.
- `SteamTools::updateShortcuts` returns `bool`, refuses to write when the Steam user directory is unknown, writes atomically through `QSaveFile`, re-reads the file afterwards, and keeps only the five newest backups.
- The signed-in user is read from `loginusers.vdf` with a real VDF parser, preferring `MostRecent`, then `AllowAutoLogin`, then the newest `Timestamp`, with a fallback to the only `userdata/<id>` directory.
- `SteamTools::launchedFromSteam()` tells callers when the Steam client is certainly running (Game Mode), since Steam then holds its own copy of `shortcuts.vdf` and will write it back over ours.

`crc.h` and `vdf_parser.hpp` are unchanged from upstream.

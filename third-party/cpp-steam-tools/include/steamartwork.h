#ifndef STEAMARTWORK_H
#define STEAMARTWORK_H

#include "cpp-steam-tools_global.h"

#include <QMap>
#include <QPixmap>
#include <QString>

// Composes the images Steam shows for a non-Steam shortcut from whatever
// store art is available: a square cover and, if possible, a 16:9
// screenshot. Produces "hero" (1920x620), "landscape" (920x430),
// "portrait" (600x900, the cover filling the slot) and "icon" (256x256). No "logo" is produced on
// purpose: without one Steam prints the game's name over the hero, which
// beats a wrong logo.
namespace SteamArtwork {

// Scale to fill w x h and crop the overflow around the centre.
CPPSTEAMTOOLS_EXPORT QPixmap coverCrop(const QPixmap &source, int w, int h);

// A soft, darkened version of `source` filling w x h, for behind a centred
// image whose shape does not match the slot.
CPPSTEAMTOOLS_EXPORT QPixmap backdrop(const QPixmap &source, int w, int h, int darkenAlpha = 150);

// `foreground` scaled to fit within w x h (keeping aspect) and centred on
// `backdrop(behind, w, h)`.
CPPSTEAMTOOLS_EXPORT QPixmap centred(const QPixmap &foreground, const QPixmap &behind, int w, int h);

// Keys: hero, landscape, portrait, icon. `screenshot` may be null, in which
// case the cover stands in everywhere. Returns an empty map if `cover` is null.
CPPSTEAMTOOLS_EXPORT QMap<QString, QPixmap> compose(const QPixmap &cover, const QPixmap &screenshot);

} // namespace SteamArtwork

#endif // STEAMARTWORK_H

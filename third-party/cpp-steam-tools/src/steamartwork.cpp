#include "../include/steamartwork.h"

#include <QPainter>

namespace SteamArtwork {

QPixmap coverCrop(const QPixmap &source, int w, int h)
{
	if(source.isNull() || w <= 0 || h <= 0)
		return QPixmap();
	QPixmap scaled = source.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
	const int x = (scaled.width() - w) / 2;
	const int y = (scaled.height() - h) / 2;
	return scaled.copy(x, y, w, h);
}

QPixmap backdrop(const QPixmap &source, int w, int h, int darkenAlpha)
{
	if(source.isNull() || w <= 0 || h <= 0)
		return QPixmap();
	// Blur by scaling far down and back up in steps; cheap and good enough
	// for something that sits behind a sharp image.
	QPixmap filled = coverCrop(source, w, h);
	const int longest = qMax(w, h);
	QPixmap small = filled.scaled(qMax(2, longest / 80), qMax(2, longest / 80), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QPixmap mid = small.scaled(qMax(2, longest / 40), qMax(2, longest / 40), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QPixmap blurred = mid.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	QPainter painter(&blurred);
	painter.fillRect(blurred.rect(), QColor(0, 0, 0, qBound(0, darkenAlpha, 255)));
	painter.end();
	return blurred;
}

QPixmap centred(const QPixmap &foreground, const QPixmap &behind, int w, int h)
{
	if(foreground.isNull() || w <= 0 || h <= 0)
		return QPixmap();
	QPixmap canvas = backdrop(behind.isNull() ? foreground : behind, w, h);
	QPixmap fg = foreground.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QPainter painter(&canvas);
	painter.drawPixmap((w - fg.width()) / 2, (h - fg.height()) / 2, fg);
	painter.end();
	return canvas;
}

QMap<QString, QPixmap> compose(const QPixmap &cover, const QPixmap &screenshot)
{
	QMap<QString, QPixmap> out;
	if(cover.isNull())
		return out;

	const QPixmap &wide = screenshot.isNull() ? cover : screenshot;

	// The hero is a wide banner behind the title; a screenshot cropped to
	// shape fills it, a square cover would be letterboxed.
	out["hero"] = screenshot.isNull() ? centred(cover, cover, 1920, 620) : coverCrop(screenshot, 1920, 620);
	// Capsules need the cover recognisable, so it sits on a soft wide
	// backdrop instead of being cropped.
	out["landscape"] = centred(cover, wide, 920, 430);
	out["portrait"] = centred(cover, wide, 600, 900);
	out["icon"] = coverCrop(cover, 256, 256);
	return out;
}

} // namespace SteamArtwork

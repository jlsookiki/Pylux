// Checks on the composed Steam artwork with solid-colour fixtures, so a
// pixel either is the source colour or it is not: every slot is the size
// Steam expects, a 16:9 screenshot fills the hero edge to edge, the cover
// fills the portrait slot and sits whole in the wide capsule, and a
// cover-only input still fills every slot.

#include "steamartwork.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPen>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

static const QColor COVER(220, 30, 30);
static const QColor SHOT(30, 60, 220);

static QPixmap solid(int w, int h, const QColor &c)
{
	QImage img(w, h, QImage::Format_RGB32);
	img.fill(c);
	return QPixmap::fromImage(img);
}

static bool is(const QImage &img, int x, int y, const QColor &c)
{
	return img.pixelColor(x, y) == c;
}

static void testSizes()
{
	const QMap<QString, QPixmap> art = SteamArtwork::compose(solid(1000, 1000, COVER), solid(1920, 1080, SHOT));
	CHECK(art.size() == 4);
	CHECK(!art.contains("logo"));
	CHECK(art.value("hero").size() == QSize(1920, 620));
	CHECK(art.value("landscape").size() == QSize(920, 430));
	CHECK(art.value("portrait").size() == QSize(600, 900));
	CHECK(art.value("icon").size() == QSize(256, 256));
}

static void testHeroIsTheScreenshotEdgeToEdge()
{
	const QImage hero = SteamArtwork::compose(solid(1000, 1000, COVER), solid(1920, 1080, SHOT)).value("hero").toImage();
	// Cropped, not letterboxed: the screenshot colour reaches both edges and
	// the cover never appears.
	CHECK(is(hero, 0, 0, SHOT));
	CHECK(is(hero, 1919, 619, SHOT));
	CHECK(is(hero, 960, 310, SHOT));
	CHECK(!is(hero, 960, 310, COVER));
}

static void testPortraitIsTheCoverEdgeToEdge()
{
	// The cover fills the 600x900 slot: every corner is cover colour, no band.
	const QImage portrait = SteamArtwork::compose(solid(1000, 1000, COVER), solid(1920, 1080, SHOT)).value("portrait").toImage();
	CHECK(is(portrait, 0, 0, COVER));
	CHECK(is(portrait, 599, 0, COVER));
	CHECK(is(portrait, 0, 899, COVER));
	CHECK(is(portrait, 599, 899, COVER));
	CHECK(is(portrait, 300, 450, COVER));
	// And it is a crop, not a stretch: a cover with a distinct left edge
	// loses that edge rather than squeezing it in.
	QImage marked(1000, 1000, QImage::Format_RGB32);
	marked.fill(COVER);
	for(int y = 0; y < 1000; y++)
		for(int x = 0; x < 100; x++)
			marked.setPixelColor(x, y, SHOT);
	const QImage cropped = SteamArtwork::compose(QPixmap::fromImage(marked), QPixmap()).value("portrait").toImage();
	CHECK(is(cropped, 0, 450, COVER));
}

static void testLandscapeCentresTheCover()
{
	const QImage landscape = SteamArtwork::compose(solid(1000, 1000, COVER), solid(1920, 1080, SHOT)).value("landscape").toImage();
	// 920x430 slot, cover scaled to 430x430 at x 245..674.
	CHECK(is(landscape, 460, 215, COVER));
	CHECK(is(landscape, 245, 215, COVER));
	CHECK(is(landscape, 674, 215, COVER));
	CHECK(!is(landscape, 244, 215, COVER));
	CHECK(!is(landscape, 675, 215, COVER));
	CHECK(!is(landscape, 20, 215, COVER));
}

static void testIconIsTheCover()
{
	const QImage icon = SteamArtwork::compose(solid(1000, 1000, COVER), solid(1920, 1080, SHOT)).value("icon").toImage();
	CHECK(is(icon, 0, 0, COVER));
	CHECK(is(icon, 255, 255, COVER));
}

static void testCoverOnlyFillsEverySlot()
{
	const QMap<QString, QPixmap> art = SteamArtwork::compose(solid(800, 800, COVER), QPixmap());
	CHECK(art.size() == 4);
	const QImage hero = art.value("hero").toImage();
	CHECK(hero.size() == QSize(1920, 620));
	// Cover centred (620x620 at x 650..1269) on a backdrop made from itself:
	// the edges are a darkened red, not black and not the raw cover.
	CHECK(is(hero, 960, 310, COVER));
	CHECK(!is(hero, 10, 310, COVER));
	const QColor edge = hero.pixelColor(10, 310);
	CHECK(edge.red() > 40 && edge.green() < 40 && edge.blue() < 40);
}

// A row of hollow glyphs reads as a title; the crop window then keeps it.
static const int GLYPH_WIDTH = 50;
static const int GLYPH_GAP = 20;

static QImage coverWithGlyphs(int firstX, int count)
{
	QImage img(1000, 1000, QImage::Format_RGB32);
	img.fill(QColor(20, 20, 30));
	QPainter p(&img);
	p.setPen(QPen(Qt::white, 4));
	for(int i = 0; i < count; i++)
		p.drawRect(firstX + i * (GLYPH_WIDTH + GLYPH_GAP), 450, GLYPH_WIDTH, 70);
	return img;
}

static void testTitleWindowKeepsAnOffCentreTitle()
{
	// Six glyphs at x 60..470 of a 1000-wide cover; a 683-wide centre crop
	// (158..841) would cut them, so the window slides left to hold them.
	const int off = SteamArtwork::titleWindow(coverWithGlyphs(60, 6), 683);
	CHECK(off >= 0 && off <= 60);
	// The composed portrait then shows the first glyph instead of losing it.
	const QImage portrait = SteamArtwork::compose(QPixmap::fromImage(coverWithGlyphs(60, 6)), QPixmap()).value("portrait").toImage();
	bool whiteOnLeft = false;
	for(int x = 0; x < 100 && !whiteOnLeft; x++)
		for(int y = 380; y < 480; y++)
			if(portrait.pixelColor(x, y).lightness() > 200)
			{
				whiteOnLeft = true;
				break;
			}
	CHECK(whiteOnLeft);
}

static void testTitleWindowCentresOnATitleTooWideToFit()
{
	// Glyphs spanning 300..1000 cannot fit a 683 window; it centres on them
	// (650 - 341 = 309) rather than on the image.
	const int off = SteamArtwork::titleWindow(coverWithGlyphs(300, 10), 683);
	CHECK(off >= 295 && off <= 317);
}

static void testTitleWindowWidensOverAFusedWord()
{
	// Six glyphs at 400..810 fit the centre window (158..841), but a wide
	// hollow block at 100..380 on the same row is a word the edge map fused
	// into one shape. The window must not slice it: the span widens to
	// 100..810, which no longer fits, so the crop centres on it (114).
	QImage img = coverWithGlyphs(400, 6);
	QPainter p(&img);
	p.setPen(QPen(Qt::white, 4));
	p.drawRect(100, 450, 280, 70);
	p.end();
	const int off = SteamArtwork::titleWindow(img, 683);
	CHECK(off >= 100 && off <= 130);
}

static void testTitleWindowStaysCentredWithoutATitle()
{
	QImage plain(1000, 1000, QImage::Format_RGB32);
	plain.fill(COVER);
	CHECK(SteamArtwork::titleWindow(plain, 683) == 158);
	// Three glyphs are not enough to count as a title.
	CHECK(SteamArtwork::titleWindow(coverWithGlyphs(60, 3), 683) == 158);
	// A crop that loses no width needs no window at all.
	CHECK(SteamArtwork::titleWindow(plain, 1000) == 0);
}

static void testNullCover()
{
	CHECK(SteamArtwork::compose(QPixmap(), solid(1920, 1080, SHOT)).isEmpty());
	CHECK(SteamArtwork::coverCrop(QPixmap(), 10, 10).isNull());
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QGuiApplication app(argc, argv);
	testSizes();
	testHeroIsTheScreenshotEdgeToEdge();
	testPortraitIsTheCoverEdgeToEdge();
	testLandscapeCentresTheCover();
	testIconIsTheCover();
	testCoverOnlyFillsEverySlot();
	testTitleWindowKeepsAnOffCentreTitle();
	testTitleWindowCentresOnATitleTooWideToFit();
	testTitleWindowWidensOverAFusedWord();
	testTitleWindowStaysCentredWithoutATitle();
	testNullCover();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamartwork: all checks passed\n");
	return 0;
}

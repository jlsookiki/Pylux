// Shape checks for the composed Steam artwork: every slot is the size
// Steam expects, a 16:9 screenshot fills the hero edge to edge instead of
// being letterboxed, and a square cover is never stretched.

#include "steamartwork.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

// A bright, non-uniform test image so crops and letterboxing are visible.
static QPixmap testImage(int w, int h, const QColor &base)
{
	QImage img(w, h, QImage::Format_ARGB32);
	QPainter p(&img);
	QLinearGradient g(0, 0, w, h);
	g.setColorAt(0, base);
	g.setColorAt(1, Qt::white);
	p.fillRect(img.rect(), g);
	p.end();
	return QPixmap::fromImage(img);
}

static bool columnIsBlack(const QImage &img, int x)
{
	for(int y = 0; y < img.height(); y += 7)
		if(qGray(img.pixel(x, y)) > 16)
			return false;
	return true;
}

static void testSizesAndHeroFill()
{
	const QPixmap cover = testImage(1000, 1000, Qt::red);
	const QPixmap shot = testImage(1920, 1080, Qt::blue);

	const QMap<QString, QPixmap> art = SteamArtwork::compose(cover, shot);
	CHECK(art.size() == 4);
	CHECK(!art.contains("logo"));
	CHECK(art.value("hero").size() == QSize(1920, 620));
	CHECK(art.value("landscape").size() == QSize(920, 430));
	CHECK(art.value("portrait").size() == QSize(600, 900));
	CHECK(art.value("icon").size() == QSize(256, 256));

	// The screenshot fills the hero: no black pillars at the edges.
	const QImage hero = art.value("hero").toImage();
	CHECK(!columnIsBlack(hero, 2));
	CHECK(!columnIsBlack(hero, hero.width() - 3));

	// The portrait keeps the square cover square (600 wide, 600 tall) over
	// a darker backdrop rather than stretching it to 600x900.
	const QImage portrait = art.value("portrait").toImage();
	const int topBand = qGray(portrait.pixel(300, 40));
	const int middle = qGray(portrait.pixel(300, 450));
	CHECK(middle > topBand);
}

static void testCoverOnlyStillProducesEverything()
{
	const QPixmap cover = testImage(800, 800, Qt::green);
	const QMap<QString, QPixmap> art = SteamArtwork::compose(cover, QPixmap());
	CHECK(art.size() == 4);
	CHECK(art.value("hero").size() == QSize(1920, 620));
	// With only a square cover the hero cannot be filled by it; the backdrop
	// must still cover the edges so there is no hard black.
	const QImage hero = art.value("hero").toImage();
	CHECK(!columnIsBlack(hero, 2));
}

static void testNullCover()
{
	CHECK(SteamArtwork::compose(QPixmap(), testImage(1920, 1080, Qt::blue)).isEmpty());
	CHECK(SteamArtwork::coverCrop(QPixmap(), 10, 10).isNull());
}

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QGuiApplication app(argc, argv);
	testSizesAndHeroFill();
	testCoverOnlyStillProducesEverything();
	testNullCover();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamartwork: all checks passed\n");
	return 0;
}

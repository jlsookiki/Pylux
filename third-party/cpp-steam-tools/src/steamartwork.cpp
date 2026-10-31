#include "../include/steamartwork.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

namespace SteamArtwork {

// --- Title detection --------------------------------------------------------
//
// A cover's title is a row of letters: compact edge outlines of similar
// height, aligned top and bottom, evenly spaced, with quiet rows above and
// below. Finding such rows in a thumbnail's edge map needs no OCR, and the
// rules are strict on purpose: when nothing qualifies the crop stays at the
// centre, which is never worse than today, while a misread would move it
// somewhere wrong.

namespace {

constexpr int ANALYSIS_WIDTH = 384;
constexpr float EDGE_THRESHOLD = 60.0f;
constexpr int MIN_LETTERS = 4;

struct Box
{
	int x0, x1, y0, y1; // half-open
	int pts;
	int width() const { return x1 - x0; }
	int height() const { return y1 - y0; }
};

struct Mask
{
	int w = 0, h = 0;
	std::vector<unsigned char> on;
	float density(int x0, int x1, int y0, int y1) const
	{
		x0 = std::max(x0, 0); y0 = std::max(y0, 0); x1 = std::min(x1, w); y1 = std::min(y1, h);
		if(x1 <= x0 || y1 <= y0)
			return 0.0f;
		long sum = 0;
		for(int y = y0; y < y1; y++)
			for(int x = x0; x < x1; x++)
				sum += on[y * w + x];
		return float(sum) / float((x1 - x0) * (y1 - y0));
	}
};

Mask edgeMask(const QImage &source)
{
	const QImage gray = source.scaledToWidth(ANALYSIS_WIDTH, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
	Mask m;
	m.w = gray.width();
	m.h = gray.height();
	m.on.assign(size_t(m.w) * m.h, 0);
	for(int y = 1; y < m.h - 1; y++)
	{
		const uchar *above = gray.constScanLine(y - 1);
		const uchar *row = gray.constScanLine(y);
		const uchar *below = gray.constScanLine(y + 1);
		for(int x = 1; x < m.w - 1; x++)
		{
			const float gx = float(row[x + 1]) - float(row[x - 1]);
			const float gy = float(below[x]) - float(above[x]);
			m.on[y * m.w + x] = std::hypot(gx, gy) > EDGE_THRESHOLD;
		}
	}
	return m;
}

// 8-connected components of the mask, each as a bounding box and pixel count.
std::vector<Box> components(const Mask &m)
{
	std::vector<unsigned char> seen(m.on.size(), 0);
	std::vector<Box> out;
	std::vector<int> stack;
	for(int sy = 0; sy < m.h; sy++)
	{
		for(int sx = 0; sx < m.w; sx++)
		{
			const int start = sy * m.w + sx;
			if(!m.on[start] || seen[start])
				continue;
			Box b{sx, sx + 1, sy, sy + 1, 0};
			seen[start] = 1;
			stack.assign(1, start);
			while(!stack.empty())
			{
				const int idx = stack.back();
				stack.pop_back();
				const int cx = idx % m.w, cy = idx / m.w;
				b.pts++;
				b.x0 = std::min(b.x0, cx); b.x1 = std::max(b.x1, cx + 1);
				b.y0 = std::min(b.y0, cy); b.y1 = std::max(b.y1, cy + 1);
				for(int ny = cy - 1; ny <= cy + 1; ny++)
				{
					for(int nx = cx - 1; nx <= cx + 1; nx++)
					{
						if(nx < 0 || ny < 0 || nx >= m.w || ny >= m.h)
							continue;
						const int n = ny * m.w + nx;
						if(m.on[n] && !seen[n])
						{
							seen[n] = 1;
							stack.push_back(n);
						}
					}
				}
			}
			out.push_back(b);
		}
	}
	return out;
}

// A letter is a compact outline: not a sliver, not a solid blob, and a
// plausible fraction of the image's height.
bool isLetter(const Box &b, int imageHeight)
{
	const float cw = float(b.width()), ch = float(b.height());
	if(ch < 0.015f * imageHeight || ch > 0.15f * imageHeight)
		return false;
	if(cw < 0.3f * ch || cw > 2.2f * ch)
		return false;
	return float(b.pts) >= 0.8f * ch && float(b.pts) <= 0.6f * cw * ch;
}

struct Line
{
	int x0, x1, y0, y1;
	int letters;
	long area() const { return long(x1 - x0) * (y1 - y0); }
};

Line boundsOf(const std::vector<Box> &chain)
{
	Line line{chain[0].x0, chain[0].x1, chain[0].y0, chain[0].y1, int(chain.size())};
	for(const Box &b : chain)
	{
		line.x0 = std::min(line.x0, b.x0); line.x1 = std::max(line.x1, b.x1);
		line.y0 = std::min(line.y0, b.y0); line.y1 = std::max(line.y1, b.y1);
	}
	return line;
}

// Type is set at a steady pitch; scattered detail of letter size is not.
bool evenlySpaced(const std::vector<Box> &chain)
{
	float meanHeight = 0, meanGap = 0;
	for(size_t k = 0; k < chain.size(); k++)
	{
		meanHeight += chain[k].height();
		if(k)
			meanGap += chain[k].x0 - chain[k - 1].x1;
	}
	meanHeight /= chain.size();
	meanGap /= float(chain.size() - 1);
	float gapVar = 0;
	for(size_t k = 1; k < chain.size(); k++)
		gapVar += std::pow(float(chain[k].x0 - chain[k - 1].x1) - meanGap, 2.0f);
	return std::sqrt(gapVar / float(chain.size() - 1)) <= 0.5f * meanHeight + 1.0f;
}

// A title has clear space on at least one side (a subtitle or rule may sit
// on the other); a row of texture has more texture both above and below.
bool inClearSpace(const Mask &m, const Line &line)
{
	// The rows touching the letters hold their own blur, so the band starts one row out.
	const int margin = std::max(2, int(std::lround(0.2 * (line.y1 - line.y0))));
	return m.density(line.x0, line.x1, line.y0 - 1 - margin, line.y0 - 1) < 0.1f || m.density(line.x0, line.x1, line.y1 + 1, line.y1 + 1 + margin) < 0.1f;
}

struct Detection
{
	std::vector<Box> boxes;  // every component, for the edge check
	std::vector<Line> lines; // title lines, largest first
};

Detection detectTitle(const Mask &m)
{
	const std::vector<Box> boxes = components(m);
	std::vector<Box> letters;
	for(const Box &b : boxes)
		if(isLetter(b, m.h))
			letters.push_back(b);
	std::sort(letters.begin(), letters.end(), [](const Box &a, const Box &b) {
		return std::tie(a.x0, a.x1, a.y0, a.y1) < std::tie(b.x0, b.x1, b.y0, b.y1);
	});

	std::vector<unsigned char> used(letters.size(), 0);
	std::vector<Line> lines;
	for(size_t i = 0; i < letters.size(); i++)
	{
		if(used[i])
			continue;
		std::vector<Box> chain{letters[i]};
		used[i] = 1;
		for(size_t j = i + 1; j < letters.size(); j++)
		{
			if(used[j])
				continue;
			const Box &last = chain.back();
			const Box &b = letters[j];
			const float hm = float(std::max(last.height(), b.height()));
			const float hmin = float(std::min(last.height(), b.height()));
			// Downscaling blurs neighbours together, so a slight overlap still counts as adjacent.
			const float gap = float(b.x0 - last.x1);
			if(std::abs(last.y0 - b.y0) <= 0.35f * hm && std::abs(last.y1 - b.y1) <= 0.35f * hm && hm <= 1.6f * hmin && gap >= -0.2f * hm && gap <= 1.5f * hm)
			{
				chain.push_back(b);
				used[j] = 1;
			}
		}
		if(int(chain.size()) < MIN_LETTERS || !evenlySpaced(chain))
			continue;
		const Line line = boundsOf(chain);
		if(inClearSpace(m, line))
			lines.push_back(line);
	}
	std::sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) { return a.area() > b.area(); });
	return {boxes, lines};
}

// Where to start a window of `width` so it holds [x0, x1) if it can, as
// near the middle as possible, else centred on that span.
float placeWindow(float x0, float x1, float width, float middle)
{
	if(x1 - x0 <= width)
		return std::min(std::max(middle, x1 - width), x0);
	return (x0 + x1) / 2.0f - width / 2.0f;
}

// A box of letter height on the title's row that the window's edge would
// slice. Downscaling can fuse a whole word into one box, so the shape is
// not checked, only the height and the row.
bool cutByWindow(const Box &b, const Line &title, float left, float right)
{
	const int titleHeight = title.y1 - title.y0;
	if(b.height() < titleHeight / 2 || b.height() > titleHeight * 3 / 2)
		return false;
	const int overlap = std::min(b.y1, title.y1) - std::max(b.y0, title.y0);
	if(overlap < b.height() / 2)
		return false;
	return (float(b.x0) < left && left < float(b.x1)) || (float(b.x0) < right && right < float(b.x1));
}

} // namespace

int titleWindow(const QImage &source, int cropWidth)
{
	const int sourceWidth = source.width();
	const int centre = (sourceWidth - cropWidth) / 2;
	if(source.isNull() || cropWidth <= 0 || cropWidth >= sourceWidth)
		return std::max(0, centre);

	const Mask mask = edgeMask(source);
	const Detection found = detectTitle(mask);
	if(found.lines.empty())
		return centre;

	const float scale = float(mask.w) / float(sourceWidth);
	const float window = cropWidth * scale;
	const float middle = (mask.w - window) / 2.0f;

	// The largest line is the title; other lines join it while they all fit.
	const Line &title = found.lines[0];
	int x0 = title.x0, x1 = title.x1;
	for(size_t i = 1; i < found.lines.size(); i++)
	{
		if(float(std::max(x1, found.lines[i].x1) - std::min(x0, found.lines[i].x0)) <= window)
		{
			x0 = std::min(x0, found.lines[i].x0);
			x1 = std::max(x1, found.lines[i].x1);
		}
	}

	// Detection can catch only part of a title. If the window's edge would
	// slice a letter on the title's row, that letter is part of it: widen
	// the span and place again until the edges fall in clear space.
	float offset = placeWindow(float(x0), float(x1), window, middle);
	for(int pass = 0; pass < 8; pass++)
	{
		bool widened = false;
		for(const Box &b : found.boxes)
		{
			if(cutByWindow(b, title, offset, offset + window))
			{
				x0 = std::min(x0, b.x0);
				x1 = std::max(x1, b.x1);
				widened = true;
			}
		}
		if(!widened)
			break;
		offset = placeWindow(float(x0), float(x1), window, middle);
	}
	offset = std::min(std::max(offset, 0.0f), mask.w - window);
	return std::min(std::max(int(std::lround(offset / scale)), 0), sourceWidth - cropWidth);
}

QPixmap titleCrop(const QPixmap &source, int w, int h)
{
	if(source.isNull() || w <= 0 || h <= 0)
		return QPixmap();
	QPixmap scaled = source.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
	const int x = scaled.width() > w ? titleWindow(scaled.toImage(), w) : 0;
	return scaled.copy(x, (scaled.height() - h) / 2, w, h);
}

// --- Composition ------------------------------------------------------------

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
	// The wide capsule keeps the cover whole on a soft backdrop; cropping a
	// square to 2:1 would lose most of it. The portrait capsule is close
	// enough to square that filling it keeps the art and loses only the
	// edges, which reads far better in the library grid than a band; the
	// edge it loses is the one without the title.
	out["landscape"] = centred(cover, wide, 920, 430);
	out["portrait"] = titleCrop(cover, 600, 900);
	out["icon"] = coverCrop(cover, 256, 256);
	return out;
}

} // namespace SteamArtwork

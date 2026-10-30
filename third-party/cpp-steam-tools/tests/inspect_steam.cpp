// Read-only check against the Steam install on this machine: resolves the
// signed-in user, parses the real shortcuts.vdf, and reports whether
// serialize(parse(file)) reproduces the file byte for byte. Never writes.

#include "shortcutsvdf.h"
#include "steamtools.h"

#include <QFile>
#include <QGuiApplication>

#include <cstdio>

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);

	auto info = [](const QString &m) { fprintf(stdout, "  info: %s\n", qPrintable(m)); };
	auto error = [](const QString &m) { fprintf(stdout, "  error: %s\n", qPrintable(m)); };
	SteamTools steam(info, error, argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString());

	printf("steam dir:      %s (%s)\n", qPrintable(steam.steamDir()), steam.steamExists() ? "exists" : "missing");
	printf("user id:        %s (%s)\n", qPrintable(steam.userId()), steam.hasUser() ? "userdata/config present" : "NO USER");
	printf("shortcuts.vdf:  %s\n", qPrintable(steam.shortcutFilePath()));
	printf("launched by Steam: %s\n", SteamTools::launchedFromSteam() ? "yes" : "no");

	QFile f(steam.shortcutFilePath());
	if(!f.exists())
	{
		printf("no shortcuts.vdf yet; nothing to round-trip\n");
		return steam.hasUser() ? 0 : 2;
	}
	if(!f.open(QIODevice::ReadOnly))
	{
		printf("cannot open shortcuts.vdf\n");
		return 2;
	}
	const QByteArray original = f.readAll();

	QVector<SteamShortcutEntry> entries;
	QString err;
	if(!ShortcutsVdf::parse(original, entries, &err))
	{
		printf("PARSE FAILED: %s\n", qPrintable(err));
		return 1;
	}
	printf("entries:        %d\n", int(entries.size()));
	for(const SteamShortcutEntry &e : entries)
		printf("  %-10u %-40s exe=%s  lastplay=%u  fields=%d\n", e.appid(), qPrintable(e.getAppName().left(40)),
			qPrintable(e.getExe().left(40)), e.lastPlayTime(), int(e.properties().size()));

	const QByteArray again = ShortcutsVdf::serialize(entries);
	if(again == original)
	{
		printf("round trip:     byte-exact (%d bytes)\n", int(original.size()));
		return 0;
	}
	int firstDiff = 0;
	while(firstDiff < original.size() && firstDiff < again.size() && original[firstDiff] == again[firstDiff])
		firstDiff++;
	printf("round trip:     DIFFERS at byte %d (original %d bytes, rewritten %d bytes)\n", firstDiff, int(original.size()), int(again.size()));
	return 1;
}

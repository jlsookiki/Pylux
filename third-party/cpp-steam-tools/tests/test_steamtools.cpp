// Tests for SteamTools behaviour that does not need a Steam install:
// launch-option quoting, and later the user resolution and write rules
// against a temporary Steam directory.

#include "shortcutmatch.h"
#include "steamtools.h"

#include <QCoreApplication>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

// Steam splits LaunchOptions like a shell, so a value with spaces has to
// arrive as one argument.
static void testQuoteArgument()
{
	CHECK(SteamTools::quoteArgument("Living Room") == QStringLiteral("\"Living Room\""));
	CHECK(SteamTools::quoteArgument("plain") == QStringLiteral("\"plain\""));
	CHECK(SteamTools::quoteArgument("") == QStringLiteral("\"\""));
	CHECK(SteamTools::quoteArgument("say \"hi\"") == QStringLiteral("\"say \\\"hi\\\"\""));
	CHECK(SteamTools::quoteArgument("back\\slash") == QStringLiteral("\"back\\\\slash\""));
}

// Re-adding a tile must not wipe what Steam and the user put on it.
static void testMergeKeepsUserData()
{
	SteamShortcutEntry existing;
	existing.setInt("appid", 111u);
	existing.setString("AppName", "Ghost of Yotei");
	existing.setString("Exe", "\"/old/pylux\"");
	existing.setString("StartDir", "\"/old\"");
	existing.setString("LaunchOptions", "--entitlement-id \"OLD\" cloudGameLibrary");
	existing.setInt("IsHidden", 1);
	existing.setInt("LastPlayTime", 1761782400u);
	existing.setString("SomeFutureField", "kept");
	existing.setList("tags", {"favorite", "PlayStation"});

	SteamShortcutEntry fresh;
	fresh.setInt("appid", 222u);
	fresh.setString("AppName", "Ghost of Yōtei");
	fresh.setString("Exe", "\"flatpak\"");
	fresh.setString("StartDir", "\"/home/deck\"");
	fresh.setString("icon", "/grid/222_icon.png");
	fresh.setString("LaunchOptions", "run io.pylux --entitlement-id \"NEW\" cloudGameLibrary");
	fresh.setInt("IsHidden", 0);
	fresh.setInt("LastPlayTime", 0);
	fresh.setList("tags", {"PlayStation", "Remote Play"});

	const SteamShortcutEntry merged = ShortcutMatch::merge(existing, fresh);

	// App-owned fields follow the fresh entry.
	CHECK(merged.appid() == 222u);
	CHECK(merged.getAppName() == QStringLiteral("Ghost of Yōtei"));
	CHECK(merged.getExe() == QStringLiteral("\"flatpak\""));
	CHECK(merged.getStartDir() == QStringLiteral("\"/home/deck\""));
	CHECK(merged.string("icon") == QStringLiteral("/grid/222_icon.png"));
	CHECK(merged.getLaunchOptions().contains(QStringLiteral("NEW")));

	// Everything else stays as it was.
	CHECK(merged.lastPlayTime() == 1761782400u);
	CHECK(merged.integer("IsHidden") == 1u);
	CHECK(merged.string("SomeFutureField") == QStringLiteral("kept"));
	CHECK(merged.getTags() == QStringLiteral("favorite,PlayStation,Remote Play"));
	// Field order of the existing entry is preserved; new keys append.
	CHECK(merged.properties().first().key == QStringLiteral("appid"));
	CHECK(merged.properties().last().key == QStringLiteral("icon"));
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	testQuoteArgument();
	testMergeKeepsUserData();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamtools: all checks passed\n");
	return 0;
}

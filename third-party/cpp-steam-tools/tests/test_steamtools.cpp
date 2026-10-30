// Pure-logic tests: launch-option quoting and the update-or-append rules.
// test_steamtools_dir.cpp covers behaviour that needs a Steam directory.

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

static SteamShortcutEntry shortcut(uint32_t appid, const char *name, const char *exe, const char *launch, QStringList tags = {})
{
	SteamShortcutEntry e;
	e.setInt("appid", appid);
	e.setString("AppName", name);
	e.setString("Exe", exe);
	e.setString("LaunchOptions", launch);
	if(!tags.isEmpty())
		e.setList("tags", tags);
	return e;
}

static void testFindExisting()
{
	QVector<SteamShortcutEntry> entries = {
		shortcut(1, "Elden Ring", "\"/opt/heroic\"", "--game elden", {"favorite"}),
		shortcut(2, "Ghost of Yotei", "\"flatpak\"", "run io.pylux --entitlement-id \"A\" cloudGameLibrary", {"PlayStation", "Remote Play"}),
		shortcut(3, "Pylux", "\"/old/path/pylux\"", "", {"PlayStation", "Remote Play"}),
		// Heroic as a Flatpak on a Steam Deck: same "flatpak" Exe as ours.
		shortcut(4, "Elden Ring", "\"flatpak\"", "run com.heroicgameslauncher.hgl --no-gui \"heroic://launch?appName=elden\"", {"favorite"}),
		shortcut(5, "Old Pylux Tile", "\"flatpak\"", "run io.github.ForWard_Technologies_LLC.Pylux --nickname \"PS5\" --title-id \"T\" launchTitle"),
	};

	// Same appid wins regardless of anything else.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(2, "renamed", "\"x\"", "y")) == 1);
	// Same command line, different appid (name changed): update.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "Ghost of Yōtei", "\"flatpak\"", "run io.pylux --entitlement-id \"A\" cloudGameLibrary")) == 1);
	// Same name, our tile at an old path: update.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "pylux", "\"/new/path/pylux\"", "")) == 2);
	// Same name and same "flatpak" Exe as Heroic's tile: still not ours, append.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "Elden Ring", "\"flatpak\"", "run io.github.ForWard_Technologies_LLC.Pylux --title-id \"E\" launchTitle")) == -1);
	// Same name, same Flatpak app id, no tags at all: ours, update.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "old pylux tile", "\"flatpak\"", "run io.github.ForWard_Technologies_LLC.Pylux --title-id \"Z\" launchTitle")) == 4);
	// Same name as somebody else's tile: leave it alone, append.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "Elden Ring", "\"flatpak\"", "run io.pylux --title-id \"E\" launchTitle")) == -1);
	// Nothing in common: append.
	CHECK(ShortcutMatch::findExisting(entries, shortcut(9, "GTA VI", "\"flatpak\"", "run io.pylux --entitlement-id \"G\" cloudGameLibrary")) == -1);
	CHECK(ShortcutMatch::findExisting({}, shortcut(9, "GTA VI", "\"flatpak\"", "")) == -1);
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	testQuoteArgument();
	testMergeKeepsUserData();
	testFindExisting();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamtools: all checks passed\n");
	return 0;
}

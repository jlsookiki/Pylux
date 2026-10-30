// Round-trip tests for the binary shortcuts.vdf reader/writer.
// Plain executable: exits non-zero on the first failing check.

#include "shortcutsvdf.h"

#include <QByteArray>
#include <QDebug>
#include <QVector>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

static QByteArray cstr(const char *s)
{
	return QByteArray(s) + '\0';
}

static QByteArray int32le(uint32_t v)
{
	QByteArray b;
	for(int i = 0; i < 4; i++)
		b.append(char((v >> (8 * i)) & 0xff));
	return b;
}

// A file as Steam itself writes it: one shortcut with a real LastPlayTime,
// a field this library has never heard of, and tags that are not last.
static QByteArray steamWrittenFile()
{
	QByteArray f;
	f += char(0x00); f += cstr("shortcuts");
	f += char(0x00); f += cstr("0");
	f += char(0x02); f += cstr("appid"); f += int32le(2873518374u);
	f += char(0x01); f += cstr("AppName"); f += cstr("Ghost of Yōtei");
	f += char(0x01); f += cstr("Exe"); f += cstr("\"flatpak\"");
	f += char(0x01); f += cstr("StartDir"); f += cstr("\"/home/deck\"");
	f += char(0x01); f += cstr("LaunchOptions"); f += cstr("run io.github.example --entitlement-id \"X\" cloudGameLibrary");
	f += char(0x02); f += cstr("IsHidden"); f += int32le(0);
	f += char(0x02); f += cstr("LastPlayTime"); f += int32le(1761782400u);
	f += char(0x00); f += cstr("tags");
	f += char(0x01); f += cstr("0"); f += cstr("favorite");
	f += char(0x01); f += cstr("1"); f += cstr("PlayStation");
	f += char(0x08);
	f += char(0x01); f += cstr("SomeFutureField"); f += cstr("kept verbatim");
	f += char(0x02); f += cstr("AnotherInt"); f += int32le(42);
	f += char(0x08);
	f += char(0x08);
	f += char(0x08);
	return f;
}

static void testRoundTripIsByteExact()
{
	const QByteArray input = steamWrittenFile();
	QVector<SteamShortcutEntry> entries;
	QString err;
	CHECK(ShortcutsVdf::parse(input, entries, &err));
	CHECK(err.isEmpty());
	CHECK(entries.size() == 1);
	if(entries.size() != 1)
		return;

	const SteamShortcutEntry &e = entries[0];
	CHECK(e.appid() == 2873518374u);
	CHECK(e.getAppName() == QStringLiteral("Ghost of Yōtei"));
	CHECK(e.lastPlayTime() == 1761782400u);
	CHECK(e.getTags() == QStringLiteral("favorite,PlayStation"));
	CHECK(e.string("SomeFutureField") == QStringLiteral("kept verbatim"));
	CHECK(e.integer("AnotherInt") == 42u);
	CHECK(e.integer("ishidden") == 0u);

	CHECK(ShortcutsVdf::serialize(entries) == input);
}

static void testEditPreservesOtherEntries()
{
	QVector<SteamShortcutEntry> entries;
	QString err;
	CHECK(ShortcutsVdf::parse(steamWrittenFile(), entries, &err));

	SteamShortcutEntry added;
	added.setInt("appid", 123u);
	added.setString("AppName", "GTA VI");
	added.setString("Exe", "\"flatpak\"");
	added.setList("tags", {"PlayStation"});
	entries.append(added);

	QVector<SteamShortcutEntry> again;
	CHECK(ShortcutsVdf::parse(ShortcutsVdf::serialize(entries), again, &err));
	CHECK(again.size() == 2);
	if(again.size() == 2)
	{
		CHECK(again[0].lastPlayTime() == 1761782400u);
		CHECK(again[0].string("SomeFutureField") == QStringLiteral("kept verbatim"));
		CHECK(again[1].getAppName() == QStringLiteral("GTA VI"));
		CHECK(again[1].appid() == 123u);
	}
}

static void testEmptyAndMinimalFiles()
{
	QVector<SteamShortcutEntry> entries;
	QString err;
	CHECK(ShortcutsVdf::parse(QByteArray(), entries, &err));
	CHECK(entries.isEmpty());

	QByteArray minimal;
	minimal += char(0x00); minimal += cstr("shortcuts"); minimal += char(0x08); minimal += char(0x08);
	CHECK(ShortcutsVdf::parse(minimal, entries, &err));
	CHECK(entries.isEmpty());
	CHECK(ShortcutsVdf::serialize({}) == minimal);
}

static void testMalformedIsRejectedNotTruncated()
{
	QVector<SteamShortcutEntry> entries;
	QString err;

	QByteArray truncated = steamWrittenFile();
	truncated.chop(20);
	CHECK(!ShortcutsVdf::parse(truncated, entries, &err));
	CHECK(!err.isEmpty());
	CHECK(entries.isEmpty());

	QByteArray wrongRoot;
	wrongRoot += char(0x00); wrongRoot += cstr("users"); wrongRoot += char(0x08); wrongRoot += char(0x08);
	CHECK(!ShortcutsVdf::parse(wrongRoot, entries, &err));

	QByteArray unknownType = steamWrittenFile();
	int at = unknownType.indexOf(cstr("IsHidden")) - 1;
	unknownType[at] = char(0x07);
	CHECK(!ShortcutsVdf::parse(unknownType, entries, &err));
}

static void testKeysAreCaseInsensitiveButPreserved()
{
	SteamShortcutEntry e;
	e.setString("AppName", "one");
	e.setString("appname", "two");
	CHECK(e.properties().size() == 1);
	CHECK(e.properties()[0].key == QStringLiteral("AppName"));
	CHECK(e.getAppName() == QStringLiteral("two"));
}

int main()
{
	testRoundTripIsByteExact();
	testEditPreservesOtherEntries();
	testEmptyAndMinimalFiles();
	testMalformedIsRejectedNotTruncated();
	testKeysAreCaseInsensitiveButPreserved();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("cpp-steam-tools: all checks passed\n");
	return 0;
}

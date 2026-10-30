// SteamTools against a temporary Steam directory: which signed-in user it
// picks from loginusers.vdf, when it refuses to write, and that a write
// reads back. Nothing here touches the real Steam install.

#include "shortcutsvdf.h"
#include "steamtools.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

static const QString ACCOUNT_A64 = QStringLiteral("76561197963058836"); // account id 2793108
static const QString ACCOUNT_A = QStringLiteral("2793108");
static const QString ACCOUNT_B64 = QStringLiteral("76561198000000001");
static const QString ACCOUNT_B = QStringLiteral("39734273");

static void writeFile(const QString &path, const QByteArray &data)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	f.open(QIODevice::WriteOnly | QIODevice::Truncate);
	f.write(data);
}

// A Steam dir with loginusers.vdf built from (steamid64, extra attributes)
// pairs and a userdata/<id>/config dir for each listed account id.
struct SteamDir {
	QTemporaryDir dir;
	QString path() const { return dir.path(); }

	void loginUsers(const QList<QPair<QString, QString>> &users)
	{
		QString vdf = "\"users\"\n{\n";
		for(const auto &u : users)
			vdf += QString("\t\"%1\"\n\t{\n\t\t\"AccountName\"\t\t\"someone\"\n\t\t\"PersonalName\"\t\t\"Someone\"\n%2\t}\n").arg(u.first, u.second);
		vdf += "}\n";
		writeFile(path() + "/config/loginusers.vdf", vdf.toUtf8());
	}

	void userdata(const QString &accountId)
	{
		QDir().mkpath(path() + "/userdata/" + accountId + "/config");
	}

	SteamTools tools(QStringList *errors = nullptr) const
	{
		return SteamTools([](const QString &) {}, [errors](const QString &m) { if(errors) *errors << m; }, path());
	}
};

static QString attr(const char *key, const char *value)
{
	return QString("\t\t\"%1\"\t\t\"%2\"\n").arg(key, value);
}

static void testUserPrecedence()
{
	{
		// MostRecent wins over a newer timestamp and over AllowAutoLogin.
		SteamDir d;
		d.loginUsers({{ACCOUNT_A64, attr("MostRecent", "1") + attr("Timestamp", "100")},
					  {ACCOUNT_B64, attr("AllowAutoLogin", "1") + attr("Timestamp", "200")}});
		d.userdata(ACCOUNT_A);
		d.userdata(ACCOUNT_B);
		CHECK(d.tools().userId() == ACCOUNT_A);
		CHECK(d.tools().hasUser());
	}
	{
		// No MostRecent (a Steam Deck after reboot): AllowAutoLogin.
		SteamDir d;
		d.loginUsers({{ACCOUNT_A64, attr("Timestamp", "300")},
					  {ACCOUNT_B64, attr("AllowAutoLogin", "1") + attr("Timestamp", "200")}});
		d.userdata(ACCOUNT_A);
		d.userdata(ACCOUNT_B);
		CHECK(d.tools().userId() == ACCOUNT_B);
	}
	{
		// Neither flag (a Mac that only remembers the password): newest Timestamp.
		SteamDir d;
		d.loginUsers({{ACCOUNT_A64, attr("RememberPassword", "1") + attr("Timestamp", "100")},
					  {ACCOUNT_B64, attr("RememberPassword", "1") + attr("Timestamp", "200")}});
		d.userdata(ACCOUNT_A);
		d.userdata(ACCOUNT_B);
		CHECK(d.tools().userId() == ACCOUNT_B);
	}
	{
		// loginusers.vdf names an account with no userdata dir, and exactly
		// one other account has one: use that one.
		SteamDir d;
		d.loginUsers({{ACCOUNT_B64, attr("MostRecent", "1")}});
		d.userdata(ACCOUNT_A);
		d.userdata("0"); // Steam's anonymous placeholder must not count
		CHECK(d.tools().userId() == ACCOUNT_A);
	}
	{
		// No loginusers.vdf and two candidate dirs: ambiguous, so no user.
		SteamDir d;
		d.userdata(ACCOUNT_A);
		d.userdata(ACCOUNT_B);
		CHECK(!d.tools().hasUser());
	}
}

static SteamShortcutEntry sampleShortcut(uint32_t appid, const char *name)
{
	SteamShortcutEntry e;
	e.setInt("appid", appid);
	e.setString("AppName", name);
	e.setString("Exe", "\"flatpak\"");
	e.setString("LaunchOptions", "run io.pylux cloudGameLibrary");
	e.setList("tags", {"PlayStation", "Remote Play"});
	return e;
}

static void testRefusesWithoutUser()
{
	SteamDir d;
	QStringList errors;
	SteamTools tools = d.tools(&errors);
	CHECK(!tools.hasUser());
	CHECK(!tools.updateShortcuts({sampleShortcut(1, "GTA VI")}));
	CHECK(!tools.lastError().isEmpty());
	CHECK(!errors.isEmpty());
	CHECK(!QFile::exists(d.path() + "/userdata//config/shortcuts.vdf"));
	CHECK(QDir(d.path() + "/userdata").entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
}

static void testRefusesToReplaceUnparseableFile()
{
	SteamDir d;
	d.loginUsers({{ACCOUNT_A64, attr("MostRecent", "1")}});
	d.userdata(ACCOUNT_A);
	const QString file = d.path() + "/userdata/" + ACCOUNT_A + "/config/shortcuts.vdf";
	const QByteArray garbage("\x00shortcuts\x00\x00" "0\x00\x07what\x00", 20);
	writeFile(file, garbage);

	QStringList errors;
	SteamTools tools = d.tools(&errors);
	QVector<SteamShortcutEntry> parsed;
	CHECK(!tools.parseShortcuts(parsed));
	CHECK(!tools.updateShortcuts({sampleShortcut(1, "GTA VI")}));

	QFile f(file);
	f.open(QIODevice::ReadOnly);
	CHECK(f.readAll() == garbage);
}

static void testWriteReadsBackAndKeepsFiveBackups()
{
	SteamDir d;
	d.loginUsers({{ACCOUNT_A64, attr("MostRecent", "1")}});
	d.userdata(ACCOUNT_A);
	SteamTools tools = d.tools();
	CHECK(tools.hasUser());

	QVector<SteamShortcutEntry> entries = {sampleShortcut(1, "GTA VI")};
	CHECK(tools.updateShortcuts(entries));
	QVector<SteamShortcutEntry> back;
	CHECK(tools.parseShortcuts(back));
	CHECK(back.size() == 1 && back[0].getAppName() == QStringLiteral("GTA VI"));

	// Each later write backs up the previous file, keeping the newest five.
	for(int i = 2; i <= 9; i++)
	{
		entries.append(sampleShortcut(i, "Another"));
		CHECK(tools.updateShortcuts(entries));
	}
	CHECK(tools.parseShortcuts(back));
	CHECK(back.size() == 9);
	const QDir configDir(d.path() + "/userdata/" + ACCOUNT_A + "/config");
	CHECK(configDir.entryList({"shortcuts.vdf.*.bak"}, QDir::Files).size() == 5);
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	testUserPrecedence();
	testRefusesWithoutUser();
	testRefusesToReplaceUnparseableFile();
	testWriteReadsBackAndKeepsFiveBackups();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamtools dir: all checks passed\n");
	return 0;
}

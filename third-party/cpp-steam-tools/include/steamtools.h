#ifndef STEAMTOOLS_H
#define STEAMTOOLS_H

#include "cpp-steam-tools_global.h"
#include "steamshortcutentry.h"

#include <QMap>
#include <QPixmap>
#include <QString>
#include <QVector>

#include <functional>

// Adds and updates non-Steam shortcuts for the signed-in Steam user.
//
// Every write goes through updateShortcuts(), which refuses to touch the
// file unless the user directory is known and the existing file parsed
// completely, writes atomically, and re-reads the result before reporting
// success. Diagnostics go to the info/error callbacks; the return values
// are what callers should trust.
class CPPSTEAMTOOLS_EXPORT SteamTools
{
public:
	using LogFn = std::function<void(const QString &)>;

	SteamTools(const LogFn &infoFunction, const LogFn &errorFunction, const QString &steamDir);

	bool steamExists() const;
	// True when the signed-in user was resolved and userdata/<id>/config exists.
	bool hasUser() const;
	QString userId() const { return mostRecentUser; }
	QString steamDir() const { return steamBaseDir; }
	QString shortcutFilePath() const { return shortcutFile; }
	QString lastError() const { return lastErr; }

	// False if the file exists but could not be parsed completely.
	bool parseShortcuts(QVector<SteamShortcutEntry> &out);
	// Compatibility overload: empty on failure, see lastError().
	QVector<SteamShortcutEntry> parseShortcuts();

	SteamShortcutEntry buildShortcutEntry(QString appName, QString filepath, QString launchOptions, QMap<QString, const QPixmap *> artwork);
	bool updateShortcuts(const QVector<SteamShortcutEntry> &shortcuts);
	bool updateControllerConfig(QString appname, QString controllerConfigID);

	QString generateShortAppId(QString exe, QString appname);
	uint32_t generateShortcutId(QString exe, QString appname);

	// The process was started by the Steam client (Game Mode on a Steam
	// Deck, or any "non-Steam game" launch). Steam is then certainly
	// running and holds its own copy of shortcuts.vdf in memory.
	static bool launchedFromSteam();
	// Wraps a value for Steam's LaunchOptions, which Steam splits like a
	// shell: "Living Room" must arrive as one argument.
	static QString quoteArgument(const QString &value);
	static QString defaultSteamBaseDir();

private:
	QString steamBaseDir;
	QString mostRecentUser;
	QString shortcutFile;
	QString lastErr;

	LogFn infoFunction;
	LogFn errorFunction;

	QString resolveUser();
	QString userFromLoginUsers();
	QString userFromUserdataDir();
	QString gridDir() const;
	void saveArtwork(const QString &shortAppId, const QMap<QString, const QPixmap *> &artwork, QString *iconPath);
	bool backupFile(const QString &path, const QString &prefix, int keep);
	bool writeAtomically(const QString &path, const QByteArray &data);
	bool error(const QString &msg);
};

#endif // STEAMTOOLS_H

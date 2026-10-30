#ifndef CHIAKI_STEAMSHORTCUTWRITER_H
#define CHIAKI_STEAMSHORTCUTWRITER_H

#include <QMap>
#include <QPixmap>
#include <QString>

#include <functional>

struct SteamShortcutOutcome
{
	bool ok = false;
	bool updatedExisting = false;
	// Steam started this process, so the Steam client is certainly running.
	bool launchedFromSteam = false;
	// One or two user-facing sentences, including the launched-from-Steam caveat.
	QString message;
};

// The one place that turns "put this app in the Steam library" into a
// verified shortcuts.vdf change. All three shortcut flows (the Pylux app
// itself, console games, cloud games) go through write().
class SteamShortcutWriter
{
public:
	using Log = std::function<void(const QString &)>;

	// How Steam has to start this build: the host "flatpak" command, the
	// AppImage, the extracted bundle's launch.sh, or the binary itself.
	// Prepends "run <id> " to launchOptions for Flatpak.
	static QString resolveExecutable(QString &launchOptions);

	// "--profile=<name> " when a non-default settings profile is active,
	// so a shortcut launched from Steam finds the same PSN sign-in.
	static QString profileOption(const QString &profile);

	static SteamShortcutOutcome write(const QString &steamDir, const QString &appName, const QString &executable,
		const QString &launchOptions, const QMap<QString, const QPixmap *> &artwork, const Log &log);
};

#endif // CHIAKI_STEAMSHORTCUTWRITER_H

#include "steamshortcutwriter.h"

#include "steamtools.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>
#include <QVector>

static const QString STEAM_DECK_CONTROLLER_LAYOUT = QStringLiteral("3049833406");

QString SteamShortcutWriter::resolveExecutable(QString &launchOptions)
{
	QString executable = QCoreApplication::applicationFilePath();
#ifdef Q_OS_LINUX
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	const QString flatpakId = env.value("FLATPAK_ID");
	if(!flatpakId.isEmpty())
	{
		// Inside the sandbox applicationFilePath() is /app/bin/..., which
		// does not exist on the host. Steam has to go through flatpak.
		launchOptions.prepend(QString("run %1 ").arg(flatpakId));
		return QStringLiteral("flatpak");
	}
	if(env.contains("APPIMAGE"))
		return env.value("APPIMAGE");

	// Extracted bundle: .../pylux/usr/bin/chiaki -> .../pylux/launch.sh sets
	// up library paths the Steam runtime needs.
	QFileInfo exeInfo(executable);
	if(exeInfo.absoluteFilePath().contains("/usr/bin/"))
	{
		QDir dir(exeInfo.absolutePath());
		if(dir.cdUp() && dir.cdUp())
		{
			const QString launchScript = dir.absoluteFilePath("launch.sh");
			if(QFile::exists(launchScript))
				return launchScript;
		}
	}
#endif
	return executable;
}

QString SteamShortcutWriter::profileOption(const QString &profile)
{
	return profile.isEmpty() ? QString() : QString("--profile=%1 ").arg(SteamTools::quoteArgument(profile));
}

SteamShortcutOutcome SteamShortcutWriter::write(const QString &steamDir, const QString &appName, const QString &executable,
	const QString &launchOptions, const QMap<QString, const QPixmap *> &artwork, const Log &log)
{
	SteamShortcutOutcome out;
	auto fail = [&](const QString &why) {
		out.ok = false;
		out.message = why;
		if(log)
			log("[E] " + why);
		return out;
	};

	QStringList toolErrors;
	SteamTools steam(
		[&](const QString &m) { if(log) log("[I] " + m); },
		[&](const QString &m) { toolErrors << m; if(log) log("[E] " + m); },
		steamDir);

	if(!steam.steamExists())
		return fail(QString("Steam was not found at %1.").arg(steam.steamDir()));
	if(!steam.hasUser())
		return fail(QString("Could not tell which Steam account is signed in (looked in %1/userdata). Sign in to Steam on this device once, then try again.").arg(steam.steamDir()));

	QVector<SteamShortcutEntry> entries;
	if(!steam.parseShortcuts(entries))
		return fail(steam.lastError());

	SteamShortcutEntry entry = steam.buildShortcutEntry(appName, executable, launchOptions, artwork);

	// Same app id means Steam already considers it the same shortcut. Fall
	// back to the same command line, then to the same name, so re-adding
	// after a rename or a path change updates rather than duplicates.
	int index = -1;
	for(int i = 0; i < entries.size() && index < 0; i++)
		if(entries[i].appid() == entry.appid()
			|| (entries[i].getExe() == entry.getExe() && entries[i].getLaunchOptions() == entry.getLaunchOptions()))
			index = i;
	for(int i = 0; i < entries.size() && index < 0; i++)
		if(entries[i].getAppName().compare(entry.getAppName(), Qt::CaseInsensitive) == 0)
			index = i;

	if(index >= 0)
	{
		entries[index] = entry;
		out.updatedExisting = true;
	}
	else
		entries.append(entry);

	if(!steam.updateShortcuts(entries))
		return fail(steam.lastError());

	QVector<SteamShortcutEntry> check;
	bool present = steam.parseShortcuts(check);
	if(present)
	{
		present = false;
		for(const SteamShortcutEntry &e : check)
			if(e.appid() == entry.appid())
				present = true;
	}
	if(!present)
		return fail(QString("Wrote %1 but could not find %2 in it afterwards.").arg(steam.shortcutFilePath(), appName));

	// Nice to have; never the reason a shortcut "failed".
	if(!steam.updateControllerConfig(appName, STEAM_DECK_CONTROLLER_LAYOUT) && log)
		log("[I] Controller layout not applied: " + steam.lastError());

	out.ok = true;
	out.launchedFromSteam = SteamTools::launchedFromSteam();
	out.message = out.updatedExisting
		? QString("Updated the Steam shortcut for %1.").arg(appName)
		: QString("Added %1 to your Steam library.").arg(appName);
	bool artworkFailed = false;
	for(const QString &e : toolErrors)
		if(e.contains("artwork", Qt::CaseInsensitive))
			artworkFailed = true;
	if(artworkFailed)
		out.message += " Some artwork could not be saved, so the tile may show without an image.";
	if(out.launchedFromSteam)
		out.message += " Steam is running right now and keeps its own copy of the library, so it may discard this change when it exits. "
					   "For a sure result on Steam Deck, add shortcuts from Desktop Mode with Steam closed.";
	else
		out.message += " It appears after Steam restarts.";
	return out;
}

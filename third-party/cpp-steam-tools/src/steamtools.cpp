#include "../include/steamtools.h"

#include "../include/crc.h"
#include "../include/shortcutsvdf.h"
#include "../include/vdf_parser.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSaveFile>

#include <fstream>
#include <memory>
#include <sstream>

namespace {

constexpr quint64 STEAMID64_BASE = 76561197960265728ULL;

QString envOrEmpty(const char *name)
{
	return QProcessEnvironment::systemEnvironment().value(QString::fromLatin1(name));
}

} // namespace

SteamTools::SteamTools(const LogFn &infoFunction, const LogFn &errorFunction, const QString &steamDir)
	: steamBaseDir(steamDir), infoFunction(infoFunction), errorFunction(errorFunction)
{
	if(steamBaseDir.isEmpty())
		steamBaseDir = defaultSteamBaseDir();
	mostRecentUser = resolveUser();
	shortcutFile = QString("%1/userdata/%2/config/shortcuts.vdf").arg(steamBaseDir, mostRecentUser);
}

QString SteamTools::defaultSteamBaseDir()
{
	const QString home = QDir::homePath();
#if defined(__APPLE__)
	return home + "/Library/Application Support/Steam";
#elif defined(_WIN32)
	return "C:/Program Files (x86)/Steam";
#else
	const QString flatpakSteam = home + "/.var/app/com.valvesoftware.Steam/data/Steam";
	if(QDir(flatpakSteam).exists())
		return flatpakSteam;
	return home + "/.steam/steam";
#endif
}

bool SteamTools::launchedFromSteam()
{
	if(!envOrEmpty("SteamAppId").isEmpty() || !envOrEmpty("SteamGameId").isEmpty()
		|| !envOrEmpty("SteamClientLaunch").isEmpty() || !envOrEmpty("STEAM_COMPAT_CLIENT_INSTALL_PATH").isEmpty())
		return true;
	return envOrEmpty("XDG_CURRENT_DESKTOP").compare("gamescope", Qt::CaseInsensitive) == 0;
}

QString SteamTools::quoteArgument(const QString &value)
{
	QString escaped = value;
	escaped.replace('\\', "\\\\");
	escaped.replace('"', "\\\"");
	return "\"" + escaped + "\"";
}

bool SteamTools::steamExists() const
{
	return QDir(steamBaseDir).exists();
}

bool SteamTools::hasUser() const
{
	return !mostRecentUser.isEmpty() && QDir(QString("%1/userdata/%2/config").arg(steamBaseDir, mostRecentUser)).exists();
}

bool SteamTools::error(const QString &msg)
{
	lastErr = msg;
	if(errorFunction)
		errorFunction(msg);
	return false;
}

QString SteamTools::resolveUser()
{
	QString id = userFromLoginUsers();
	if(!id.isEmpty() && QDir(QString("%1/userdata/%2").arg(steamBaseDir, id)).exists())
		return id;
	QString fallback = userFromUserdataDir();
	if(!fallback.isEmpty())
	{
		if(infoFunction)
			infoFunction(QString("Steam user %1 taken from the only userdata directory").arg(fallback));
		return fallback;
	}
	return id;
}

// loginusers.vdf lists every account that has signed in on this machine.
// Prefer MostRecent, then AllowAutoLogin (what a Steam Deck sets), then
// the newest Timestamp.
QString SteamTools::userFromLoginUsers()
{
	const QString path = QString("%1/config/loginusers.vdf").arg(steamBaseDir);
	std::ifstream file(path.toStdString());
	if(!file.is_open())
	{
		if(infoFunction)
			infoFunction(QString("Could not open %1").arg(path));
		return QString();
	}

	tyti::vdf::object root;
	try
	{
		root = tyti::vdf::read(file);
	}
	catch(const std::exception &e)
	{
		if(infoFunction)
			infoFunction(QString("Could not parse %1: %2").arg(path, e.what()));
		return QString();
	}

	QString mostRecent, autoLogin, newest;
	qulonglong newestTimestamp = 0;
	for(const auto &child : root.childs)
	{
		const QString steamId64 = QString::fromStdString(child.first);
		bool ok = false;
		const qulonglong id64 = steamId64.toULongLong(&ok);
		if(!ok || id64 < STEAMID64_BASE)
			continue;
		const QString accountId = QString::number(id64 - STEAMID64_BASE);

		auto attr = [&](const char *name) -> QString {
			for(const auto &a : child.second->attribs)
				if(QString::fromStdString(a.first).compare(QLatin1String(name), Qt::CaseInsensitive) == 0)
					return QString::fromStdString(a.second);
			return QString();
		};

		if(attr("MostRecent") == "1" && mostRecent.isEmpty())
			mostRecent = accountId;
		if((attr("AllowAutoLogin") == "1" || attr("AutoLogin") == "1") && autoLogin.isEmpty())
			autoLogin = accountId;
		const qulonglong ts = attr("Timestamp").toULongLong();
		if(ts >= newestTimestamp)
		{
			newestTimestamp = ts;
			newest = accountId;
		}
	}

	if(!mostRecent.isEmpty())
		return mostRecent;
	if(!autoLogin.isEmpty())
		return autoLogin;
	return newest;
}

QString SteamTools::userFromUserdataDir()
{
	QDir userdata(steamBaseDir + "/userdata");
	QStringList users;
	for(const QString &name : userdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		bool ok = false;
		name.toULongLong(&ok);
		// "0" is Steam's anonymous placeholder, not a signed-in account.
		if(ok && name != "0")
			users.append(name);
	}
	return users.size() == 1 ? users.first() : QString();
}

QString SteamTools::gridDir() const
{
	return QString("%1/userdata/%2/config/grid").arg(steamBaseDir, mostRecentUser);
}

bool SteamTools::parseShortcuts(QVector<SteamShortcutEntry> &out)
{
	out.clear();
	QFile file(shortcutFile);
	if(!file.exists())
	{
		if(infoFunction)
			infoFunction(QString("%1 does not exist yet; starting with no shortcuts").arg(shortcutFile));
		return true;
	}
	if(!file.open(QIODevice::ReadOnly))
		return error(QString("Could not read %1: %2").arg(shortcutFile, file.errorString()));

	QString parseError;
	if(!ShortcutsVdf::parse(file.readAll(), out, &parseError))
		return error(QString("%1 is not a shortcuts file this version understands (%2); not modifying it").arg(shortcutFile, parseError));
	return true;
}

QVector<SteamShortcutEntry> SteamTools::parseShortcuts()
{
	QVector<SteamShortcutEntry> out;
	if(!parseShortcuts(out))
		out.clear();
	return out;
}

void SteamTools::saveArtwork(const QString &shortAppId, const QMap<QString, const QPixmap *> &artwork, QString *iconPath)
{
	const QString dir = gridDir();
	QDir().mkpath(dir);
	const QMap<QString, QString> suffix = {
		{"icon", "_icon.png"},
		{"landscape", ".png"},
		{"portrait", "p.png"},
		{"hero", "_hero.png"},
		{"logo", "_logo.png"},
	};
	for(auto it = artwork.cbegin(); it != artwork.cend(); ++it)
	{
		if(!suffix.contains(it.key()) || !it.value() || it.value()->isNull())
			continue;
		const QString path = dir + "/" + shortAppId + suffix.value(it.key());
		if(it.value()->save(path))
		{
			if(infoFunction)
				infoFunction(QString("Saved %1 artwork to %2").arg(it.key(), path));
			if(it.key() == "icon" && iconPath)
				*iconPath = path;
		}
		else
			error(QString("Could not save %1 artwork to %2").arg(it.key(), path));
	}
}

SteamShortcutEntry SteamTools::buildShortcutEntry(QString appName, QString filepath, QString launchOptions, QMap<QString, const QPixmap *> artwork)
{
	const QString exe = "\"" + filepath + "\"";
	const QString shortAppId = generateShortAppId(exe, appName);

	QString iconPath;
	saveArtwork(shortAppId, artwork, &iconPath);

	// Steam uses StartDir as the working directory. A bare command such as
	// "flatpak" has no directory of its own, so fall back to home rather
	// than whatever this process happened to be started from.
	QFileInfo exeInfo(filepath);
	QString startDir = exeInfo.isAbsolute() ? exeInfo.absolutePath() : QDir::homePath();

	SteamShortcutEntry entry;
	entry.setInt("appid", generateShortcutId(exe, appName));
	entry.setString("AppName", appName);
	entry.setString("Exe", exe);
	entry.setString("StartDir", "\"" + startDir + "\"");
	entry.setString("icon", iconPath);
	entry.setString("ShortcutPath", "");
	entry.setString("LaunchOptions", launchOptions);
	entry.setInt("IsHidden", 0);
	entry.setInt("AllowDesktopConfig", 1);
	entry.setInt("AllowOverlay", 1);
	entry.setInt("OpenVR", 0);
	entry.setInt("Devkit", 0);
	entry.setString("DevkitGameID", "");
	entry.setInt("DevkitOverrideAppID", 0);
	entry.setInt("LastPlayTime", 0);
	entry.setString("FlatpakAppID", "");
	entry.setList("tags", {"PlayStation", "Remote Play"});
	return entry;
}

bool SteamTools::backupFile(const QString &path, const QString &prefix, int keep)
{
	QFileInfo info(path);
	if(!info.exists())
		return true;
	QDir dir = info.dir();
	const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss");
	const QString backup = dir.filePath(QString("%1.%2.bak").arg(prefix, stamp));
	QFile::remove(backup);
	if(!QFile::copy(path, backup))
		return error(QString("Could not back up %1 to %2").arg(path, backup));
	if(infoFunction)
		infoFunction(QString("Backed up %1 to %2").arg(info.fileName(), backup));

	// Oldest first; drop everything beyond the newest `keep`.
	QStringList backups = dir.entryList({prefix + ".*.bak"}, QDir::Files, QDir::Name);
	while(backups.size() > keep)
		QFile::remove(dir.filePath(backups.takeFirst()));
	return true;
}

bool SteamTools::writeAtomically(const QString &path, const QByteArray &data)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QSaveFile file(path);
	if(!file.open(QIODevice::WriteOnly))
		return error(QString("Could not open %1 for writing: %2").arg(path, file.errorString()));
	if(file.write(data) != data.size() || !file.commit())
		return error(QString("Could not write %1: %2").arg(path, file.errorString()));
	return true;
}

bool SteamTools::updateShortcuts(const QVector<SteamShortcutEntry> &shortcuts)
{
	lastErr.clear();
	if(!hasUser())
		return error(QString("Could not determine the signed-in Steam user under %1/userdata; not writing shortcuts").arg(steamBaseDir));

	// Never replace a file we could not read in full.
	QVector<SteamShortcutEntry> existing;
	if(!parseShortcuts(existing))
		return false;

	if(!backupFile(shortcutFile, "shortcuts.vdf", 5))
		return false;
	if(!writeAtomically(shortcutFile, ShortcutsVdf::serialize(shortcuts)))
		return false;

	QVector<SteamShortcutEntry> verify;
	if(!parseShortcuts(verify))
		return false;
	if(verify.size() != shortcuts.size())
		return error(QString("Wrote %1 shortcuts to %2 but read back %3").arg(shortcuts.size()).arg(shortcutFile).arg(verify.size()));
	for(int i = 0; i < shortcuts.size(); i++)
		if(verify[i].appid() != shortcuts[i].appid())
			return error(QString("Shortcut %1 did not round-trip through %2").arg(i).arg(shortcutFile));

	if(infoFunction)
		infoFunction(QString("Wrote %1 shortcuts to %2").arg(shortcuts.size()).arg(shortcutFile));
	return true;
}

bool SteamTools::updateControllerConfig(QString appName, QString controllerConfigID)
{
	lastErr.clear();
	const QString controllerFile = QString("%1/steamapps/common/Steam Controller Configs/%2/config/configset_controller_neptune.vdf")
		.arg(steamBaseDir, mostRecentUser);
	if(!QFileInfo::exists(controllerFile))
	{
		if(infoFunction)
			infoFunction("No Steam Deck controller config on this machine; skipping controller layout");
		return true;
	}

	appName = appName.toLower();
	const std::string workshop = controllerConfigID.toStdString();

	std::ifstream in(controllerFile.toStdString());
	tyti::vdf::object root;
	try
	{
		root = tyti::vdf::read(in);
	}
	catch(const std::exception &e)
	{
		return error(QString("Could not parse %1: %2").arg(controllerFile, e.what()));
	}
	in.close();

	bool changed = false;
	auto existing = root.childs.find(appName.toStdString());
	if(existing == root.childs.end())
	{
		auto entry = std::make_unique<tyti::vdf::object>();
		entry->set_name(appName.toStdString());
		entry->add_attribute("workshop", workshop);
		root.add_child(std::move(entry));
		changed = true;
	}
	else
	{
		auto &attribs = existing->second->attribs;
		if(attribs.erase("template") > 0)
			changed = true;
		if(attribs["workshop"] != workshop)
		{
			attribs["workshop"] = workshop;
			changed = true;
		}
	}

	if(!changed)
	{
		if(infoFunction)
			infoFunction(QString("Controller layout for %1 already set").arg(appName));
		return true;
	}

	if(!backupFile(controllerFile, "configset_controller_neptune", 3))
		return false;
	std::ostringstream buffer;
	tyti::vdf::write(buffer, root);
	const std::string text = buffer.str();
	if(!writeAtomically(controllerFile, QByteArray(text.data(), int(text.size()))))
		return false;
	if(infoFunction)
		infoFunction(QString("Set %1 to use controller layout %2").arg(appName, controllerConfigID));
	return true;
}

// Steam derives a non-Steam shortcut's app id from CRC32 of exe + name.
static uint64_t preliminaryId(const std::string &exe, const std::string &appname)
{
	const std::string key = exe + appname;
	crc_t crc = crc_init();
	crc = crc_update(crc, reinterpret_cast<const unsigned char *>(key.data()), key.size());
	crc = crc_finalize(crc);
	const uint64_t top = static_cast<uint64_t>(crc) | 0x80000000ULL;
	return (top << 32) | 0x02000000ULL;
}

QString SteamTools::generateShortAppId(QString exe, QString appname)
{
	return QString::number(preliminaryId(exe.toStdString(), appname.toStdString()) >> 32);
}

uint32_t SteamTools::generateShortcutId(QString exe, QString appname)
{
	return static_cast<uint32_t>((preliminaryId(exe.toStdString(), appname.toStdString()) >> 32) - 0x100000000ULL);
}

#include "../include/shortcutmatch.h"

#include <QFileInfo>
#include <QStringList>

namespace ShortcutMatch {

SteamShortcutEntry merge(const SteamShortcutEntry &existing, const SteamShortcutEntry &fresh)
{
	SteamShortcutEntry merged = existing;

	for(const char *key : {"appid", "AppName", "Exe", "StartDir", "icon", "LaunchOptions"})
	{
		const SteamShortcutProperty *p = fresh.property(key);
		if(!p)
			continue;
		if(p->type == FieldType::INT32)
			merged.setInt(p->key, p->num);
		else if(p->type == FieldType::STRING)
			merged.setString(p->key, p->str);
	}

	const SteamShortcutProperty *freshTags = fresh.property("tags");
	if(freshTags && freshTags->type == FieldType::LIST)
	{
		QStringList tags;
		const SteamShortcutProperty *oldTags = existing.property("tags");
		if(oldTags && oldTags->type == FieldType::LIST)
			tags = oldTags->list;
		for(const QString &t : freshTags->list)
			if(!tags.contains(t, Qt::CaseInsensitive))
				tags.append(t);
		merged.setList(freshTags->key, tags);
	}

	return merged;
}

// What program a tile really starts. For a Flatpak the Exe is just the
// host's "flatpak" command, shared by every Flatpak app on a Steam Deck, so
// the app id after "run" is the identity; without one we cannot tell.
static QString launcherIdentity(const SteamShortcutEntry &e)
{
	QString exe = e.getExe();
	if(exe.startsWith('"') && exe.endsWith('"') && exe.size() >= 2)
		exe = exe.mid(1, exe.size() - 2);
	if(QFileInfo(exe).fileName() != QStringLiteral("flatpak"))
		return exe;
	const QStringList tokens = e.getLaunchOptions().split(' ', Qt::SkipEmptyParts);
	if(tokens.size() >= 2 && tokens[0] == QStringLiteral("run"))
		return QStringLiteral("flatpak:") + tokens[1];
	return QString();
}

static bool isOurs(const SteamShortcutEntry &existing, const SteamShortcutEntry &fresh)
{
	const SteamShortcutProperty *tags = existing.property("tags");
	if(tags && tags->type == FieldType::LIST && tags->list.contains(QStringLiteral("Remote Play"), Qt::CaseInsensitive))
		return true;
	const QString identity = launcherIdentity(existing);
	return !identity.isEmpty() && identity == launcherIdentity(fresh);
}

int findExisting(const QVector<SteamShortcutEntry> &entries, const SteamShortcutEntry &fresh)
{
	for(int i = 0; i < entries.size(); i++)
		if(entries[i].appid() == fresh.appid())
			return i;
	for(int i = 0; i < entries.size(); i++)
		if(entries[i].getExe() == fresh.getExe() && entries[i].getLaunchOptions() == fresh.getLaunchOptions())
			return i;
	for(int i = 0; i < entries.size(); i++)
		if(entries[i].getAppName().compare(fresh.getAppName(), Qt::CaseInsensitive) == 0 && isOurs(entries[i], fresh))
			return i;
	return -1;
}

} // namespace ShortcutMatch

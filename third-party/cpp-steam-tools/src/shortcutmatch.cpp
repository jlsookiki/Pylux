#include "../include/shortcutmatch.h"

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

} // namespace ShortcutMatch

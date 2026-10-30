#include "../include/steamshortcutentry.h"

int SteamShortcutEntry::indexOf(const QString &key) const
{
	for(int i = 0; i < props.size(); i++)
		if(props[i].key.compare(key, Qt::CaseInsensitive) == 0)
			return i;
	return -1;
}

void SteamShortcutEntry::setString(const QString &key, const QString &value)
{
	int i = indexOf(key);
	if(i < 0)
	{
		props.append({key, FieldType::STRING, value, 0, {}});
		return;
	}
	props[i].type = FieldType::STRING;
	props[i].str = value;
}

void SteamShortcutEntry::setInt(const QString &key, uint32_t value)
{
	int i = indexOf(key);
	if(i < 0)
	{
		props.append({key, FieldType::INT32, {}, value, {}});
		return;
	}
	props[i].type = FieldType::INT32;
	props[i].num = value;
}

void SteamShortcutEntry::setList(const QString &key, const QStringList &value)
{
	int i = indexOf(key);
	if(i < 0)
	{
		props.append({key, FieldType::LIST, {}, 0, value});
		return;
	}
	props[i].type = FieldType::LIST;
	props[i].list = value;
}

bool SteamShortcutEntry::contains(const QString &key) const
{
	return indexOf(key) >= 0;
}

const SteamShortcutProperty *SteamShortcutEntry::property(const QString &key) const
{
	int i = indexOf(key);
	return i < 0 ? nullptr : &props[i];
}

QString SteamShortcutEntry::string(const QString &key) const
{
	const SteamShortcutProperty *p = property(key);
	return p && p->type == FieldType::STRING ? p->str : QString();
}

uint32_t SteamShortcutEntry::integer(const QString &key) const
{
	const SteamShortcutProperty *p = property(key);
	return p && p->type == FieldType::INT32 ? p->num : 0;
}

QString SteamShortcutEntry::getTags() const
{
	const SteamShortcutProperty *p = property("tags");
	return p && p->type == FieldType::LIST ? p->list.join(',') : QString();
}

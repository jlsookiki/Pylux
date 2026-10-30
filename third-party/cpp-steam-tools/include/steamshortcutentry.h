#ifndef STEAMSHORTCUTENTRY_H
#define STEAMSHORTCUTENTRY_H

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>

// Binary VDF value kinds that appear inside a shortcuts.vdf entry.
// Steam stores booleans, appids and LastPlayTime all as int32, so the
// library keeps them as int32 and never guesses a narrower type.
enum class FieldType {
	STRING,
	INT32,
	LIST
};

struct SteamShortcutProperty {
	QString key; // exactly as written in the file, e.g. "AppName"
	FieldType type = FieldType::STRING;
	QString str;
	uint32_t num = 0;
	QStringList list;
};

// One non-Steam shortcut. Properties keep file order so a parse/write
// round trip reproduces the entry byte for byte.
class SteamShortcutEntry {
public:
	SteamShortcutEntry() = default;

	void setString(const QString &key, const QString &value);
	void setInt(const QString &key, uint32_t value);
	void setList(const QString &key, const QStringList &value);

	bool contains(const QString &key) const;
	const SteamShortcutProperty *property(const QString &key) const;
	const QList<SteamShortcutProperty> &properties() const { return props; }

	QString string(const QString &key) const;
	uint32_t integer(const QString &key) const;

	uint32_t appid() const { return integer("appid"); }
	QString getAppid() const { return QString::number(appid()); }
	QString getAppName() const { return string("AppName"); }
	QString getExe() const { return string("Exe"); }
	QString getStartDir() const { return string("StartDir"); }
	QString getLaunchOptions() const { return string("LaunchOptions"); }
	QString getTags() const;
	uint32_t lastPlayTime() const { return integer("LastPlayTime"); }

private:
	int indexOf(const QString &key) const;
	QList<SteamShortcutProperty> props;
};

#endif // STEAMSHORTCUTENTRY_H

#include "../include/shortcutsvdf.h"

#include <QtEndian>

namespace {

constexpr char TYPE_OBJECT = 0x00;
constexpr char TYPE_STRING = 0x01;
constexpr char TYPE_INT32 = 0x02;
constexpr char END_OBJECT = 0x08;

class Reader {
public:
	Reader(const QByteArray &data, QString *error) : data(data), error(error) {}

	bool atEnd() const { return pos >= data.size(); }

	bool peek(char &c) const
	{
		if(atEnd())
			return false;
		c = data[pos];
		return true;
	}

	bool take(char &c)
	{
		if(!peek(c))
			return fail("unexpected end of file");
		pos++;
		return true;
	}

	bool expect(char want, const char *what)
	{
		char c;
		if(!take(c))
			return false;
		if(c != want)
			return fail(QString("expected %1 at byte %2").arg(what).arg(pos - 1));
		return true;
	}

	bool cstring(QString &out)
	{
		int end = data.indexOf('\0', pos);
		if(end < 0)
			return fail("unterminated string");
		out = QString::fromUtf8(data.constData() + pos, end - pos);
		pos = end + 1;
		return true;
	}

	bool int32(uint32_t &out)
	{
		if(pos + 4 > data.size())
			return fail("truncated int32");
		out = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + pos));
		pos += 4;
		return true;
	}

	bool fail(const QString &msg)
	{
		if(error && error->isEmpty())
			*error = msg;
		return false;
	}

private:
	const QByteArray &data;
	QString *error;
	int pos = 0;
};

// Reads the children of one shortcut entry up to and including its 08.
bool readEntry(Reader &r, SteamShortcutEntry &entry)
{
	for(;;)
	{
		char type;
		if(!r.take(type))
			return false;
		if(type == END_OBJECT)
			return true;

		QString key;
		if(!r.cstring(key))
			return false;

		switch(type)
		{
		case TYPE_STRING: {
			QString value;
			if(!r.cstring(value))
				return false;
			entry.setString(key, value);
			break;
		}
		case TYPE_INT32: {
			uint32_t value;
			if(!r.int32(value))
				return false;
			entry.setInt(key, value);
			break;
		}
		case TYPE_OBJECT: {
			// Nested objects inside an entry are lists ("tags"): string
			// children keyed by their index.
			QStringList values;
			for(;;)
			{
				char childType;
				if(!r.take(childType))
					return false;
				if(childType == END_OBJECT)
					break;
				if(childType != TYPE_STRING)
					return r.fail(QString("unsupported nested value in \"%1\"").arg(key));
				QString index, value;
				if(!r.cstring(index) || !r.cstring(value))
					return false;
				values.append(value);
			}
			entry.setList(key, values);
			break;
		}
		default:
			return r.fail(QString("unknown field type 0x%1 for \"%2\"").arg(uint8_t(type), 2, 16, QChar('0')).arg(key));
		}
	}
}

void writeCString(QByteArray &out, const QString &s)
{
	out.append(s.toUtf8());
	out.append('\0');
}

} // namespace

bool ShortcutsVdf::parse(const QByteArray &data, QVector<SteamShortcutEntry> &out, QString *error)
{
	if(error)
		error->clear();
	out.clear();

	// Steam has not written the file yet for this user.
	if(data.isEmpty())
		return true;

	Reader r(data, error);
	if(!r.expect(TYPE_OBJECT, "root object"))
		return false;
	QString root;
	if(!r.cstring(root))
		return false;
	if(root.compare("shortcuts", Qt::CaseInsensitive) != 0)
		return r.fail(QString("root key is \"%1\", not \"shortcuts\"").arg(root));

	for(;;)
	{
		char type;
		if(!r.take(type))
			return false;
		if(type == END_OBJECT)
			break;
		if(type != TYPE_OBJECT)
			return r.fail("shortcut entry is not an object");
		QString index;
		if(!r.cstring(index))
			return false;
		SteamShortcutEntry entry;
		if(!readEntry(r, entry))
			return false;
		out.append(entry);
	}

	// Closing byte of the implicit outer object. Tolerate its absence.
	char trailing;
	if(r.peek(trailing) && trailing != END_OBJECT)
		return r.fail("trailing data after shortcuts object");
	return true;
}

QByteArray ShortcutsVdf::serialize(const QVector<SteamShortcutEntry> &shortcuts)
{
	QByteArray out;
	out.append(TYPE_OBJECT);
	writeCString(out, "shortcuts");

	for(int i = 0; i < shortcuts.size(); i++)
	{
		out.append(TYPE_OBJECT);
		writeCString(out, QString::number(i));
		for(const SteamShortcutProperty &p : shortcuts[i].properties())
		{
			switch(p.type)
			{
			case FieldType::STRING:
				out.append(TYPE_STRING);
				writeCString(out, p.key);
				writeCString(out, p.str);
				break;
			case FieldType::INT32: {
				out.append(TYPE_INT32);
				writeCString(out, p.key);
				uchar le[4];
				qToLittleEndian<quint32>(p.num, le);
				out.append(reinterpret_cast<const char *>(le), 4);
				break;
			}
			case FieldType::LIST:
				out.append(TYPE_OBJECT);
				writeCString(out, p.key);
				for(int j = 0; j < p.list.size(); j++)
				{
					out.append(TYPE_STRING);
					writeCString(out, QString::number(j));
					writeCString(out, p.list[j]);
				}
				out.append(END_OBJECT);
				break;
			}
		}
		out.append(END_OBJECT);
	}

	out.append(END_OBJECT);
	out.append(END_OBJECT);
	return out;
}

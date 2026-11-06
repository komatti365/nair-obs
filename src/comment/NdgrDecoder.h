#pragma once

#include <QByteArray>
#include <QList>
#include "../common/NicoliveData.h"

namespace Nicolive {

class NdgrDecoder {
public:
	static QList<CommentItem> decodeChunkedMessages(const QByteArray &data);
	static bool parseSingleMessage(const QByteArray &data, CommentItem &outComment);

private:
	static quint64 readVarint(const char *data, int size, int &offset);
	static QByteArray readLengthDelimited(const char *data, int size, int &offset);
	static void skipField(int wireType, const char *data, int size, int &offset);

	static bool parseNicoliveMessage(const QByteArray &data, CommentItem &outComment);
	static bool parseChat(const QByteArray &data, CommentItem &outComment);
	static void parseChatModifier(const QByteArray &data, CommentModifier &outMod);
};

} // namespace Nicolive

#include "NdgrDecoder.h"
#include <QDateTime>
#include <QDebug>

namespace Nicolive {

quint64 NdgrDecoder::readVarint(const char *data, int size, int &offset)
{
    quint64 result = 0;
    int shift = 0;
    while (offset < size) {
        quint8 b = static_cast<quint8>(data[offset++]);
        result |= static_cast<quint64>(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift >= 64) break;
    }
    return result;
}

QByteArray NdgrDecoder::readLengthDelimited(const char *data, int size, int &offset)
{
    quint64 len = readVarint(data, size, offset);
    if (offset + static_cast<int>(len) > size || static_cast<int>(len) < 0) {
        return QByteArray();
    }
    QByteArray res(data + offset, static_cast<int>(len));
    offset += static_cast<int>(len);
    return res;
}

void NdgrDecoder::skipField(int wireType, const char *data, int size, int &offset)
{
    switch (wireType) {
    case 0: // Varint
        readVarint(data, size, offset);
        break;
    case 1: // 64-bit
        offset += 8;
        break;
    case 2: { // Length-delimited
        quint64 len = readVarint(data, size, offset);
        offset += static_cast<int>(len);
        break;
    }
    case 5: // 32-bit
        offset += 4;
        break;
    default:
        break;
    }
}

QList<CommentItem> NdgrDecoder::decodeChunkedMessages(const QByteArray &data)
{
    QList<CommentItem> result;
    const char *raw = data.constData();
    int size = data.size();
    int offset = 0;

    while (offset < size) {
        // In NDGR delmited streams, messages are often prefixed with varint length
        int startOffset = offset;
        quint64 msgLen = readVarint(raw, size, offset);
        if (msgLen == 0 || offset + static_cast<int>(msgLen) > size) {
            // If not delimited, fall back to parsing full buffer as a single ChunkedMessage
            offset = startOffset;
            CommentItem item;
            if (parseSingleMessage(data, item)) {
                result.append(item);
            }
            break;
        }

        QByteArray msgBytes(raw + offset, static_cast<int>(msgLen));
        offset += static_cast<int>(msgLen);

        CommentItem item;
        if (parseSingleMessage(msgBytes, item)) {
            result.append(item);
        }
    }

    return result;
}

bool NdgrDecoder::parseSingleMessage(const QByteArray &data, CommentItem &outComment)
{
    const char *raw = data.constData();
    int size = data.size();
    int offset = 0;

    bool found = false;

    while (offset < size) {
        quint64 key = readVarint(raw, size, offset);
        int fieldNumber = static_cast<int>(key >> 3);
        int wireType = static_cast<int>(key & 0x07);

        if (fieldNumber == 1 && wireType == 2) {
            // meta: Meta
            QByteArray metaBytes = readLengthDelimited(raw, size, offset);
            // Read id from meta
            const char *mRaw = metaBytes.constData();
            int mSize = metaBytes.size();
            int mOffset = 0;
            while (mOffset < mSize) {
                quint64 mKey = readVarint(mRaw, mSize, mOffset);
                int mField = static_cast<int>(mKey >> 3);
                int mWire = static_cast<int>(mKey & 0x07);
                if (mField == 1 && mWire == 2) {
                    outComment.id = QString::fromUtf8(readLengthDelimited(mRaw, mSize, mOffset));
                } else if (mField == 2 && mWire == 2) {
                    // Timestamp: seconds (field 1)
                    QByteArray tsBytes = readLengthDelimited(mRaw, mSize, mOffset);
                    int tsOffset = 0;
                    while (tsOffset < tsBytes.size()) {
                        quint64 tsKey = readVarint(tsBytes.constData(), tsBytes.size(), tsOffset);
                        if ((tsKey >> 3) == 1 && (tsKey & 0x07) == 0) {
                            outComment.date = static_cast<qint64>(readVarint(tsBytes.constData(), tsBytes.size(), tsOffset));
                        } else {
                            skipField(static_cast<int>(tsKey & 0x07), tsBytes.constData(), tsBytes.size(), tsOffset);
                        }
                    }
                } else {
                    skipField(mWire, mRaw, mSize, mOffset);
                }
            }
        } else if (fieldNumber == 2 && wireType == 2) {
            // message: NicoliveMessage
            QByteArray nicoMsg = readLengthDelimited(raw, size, offset);
            if (parseNicoliveMessage(nicoMsg, outComment)) {
                found = true;
            }
        } else {
            skipField(wireType, raw, size, offset);
        }
    }

    return found;
}

bool NdgrDecoder::parseNicoliveMessage(const QByteArray &data, CommentItem &outComment)
{
    const char *raw = data.constData();
    int size = data.size();
    int offset = 0;
    bool found = false;

    while (offset < size) {
        quint64 key = readVarint(raw, size, offset);
        int fieldNumber = static_cast<int>(key >> 3);
        int wireType = static_cast<int>(key & 0x07);

        if (fieldNumber == 1 && wireType == 2) {
            // chat: Chat
            QByteArray chatBytes = readLengthDelimited(raw, size, offset);
            if (parseChat(chatBytes, outComment)) {
                found = true;
            }
        } else if (fieldNumber == 7 && wireType == 2) {
            // simple_notification: 運営通知
            QByteArray notifBytes = readLengthDelimited(raw, size, offset);
            // Parse message text
            int nOffset = 0;
            while (nOffset < notifBytes.size()) {
                quint64 nKey = readVarint(notifBytes.constData(), notifBytes.size(), nOffset);
                if ((nKey >> 3) == 1 && (nKey & 0x07) == 2) {
                    outComment.text = QString::fromUtf8(readLengthDelimited(notifBytes.constData(), notifBytes.size(), nOffset));
                    outComment.isBroadcaster = true;
                    outComment.userName = "運営";
                    found = true;
                } else {
                    skipField(static_cast<int>(nKey & 0x07), notifBytes.constData(), notifBytes.size(), nOffset);
                }
            }
        } else {
            skipField(wireType, raw, size, offset);
        }
    }
    return found;
}

bool NdgrDecoder::parseChat(const QByteArray &data, CommentItem &outComment)
{
    const char *raw = data.constData();
    int size = data.size();
    int offset = 0;

    while (offset < size) {
        quint64 key = readVarint(raw, size, offset);
        int fieldNumber = static_cast<int>(key >> 3);
        int wireType = static_cast<int>(key & 0x07);

        switch (fieldNumber) {
        case 1: // content: string
            if (wireType == 2) {
                outComment.text = QString::fromUtf8(readLengthDelimited(raw, size, offset));
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 2: // name: string
            if (wireType == 2) {
                outComment.userName = QString::fromUtf8(readLengthDelimited(raw, size, offset));
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 3: // vpos: int32
            if (wireType == 0) {
                outComment.vpos = static_cast<qint64>(readVarint(raw, size, offset));
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 4: // account_status: enum (0=Standard, 1=Premium)
            if (wireType == 0) {
                outComment.isPremium = (readVarint(raw, size, offset) == 1);
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 5: // raw_user_id: int64
            if (wireType == 0) {
                quint64 uid = readVarint(raw, size, offset);
                outComment.userId = QString::number(uid);
                outComment.isAnonymous = false;
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 6: // hashed_user_id: string (184 anonymous)
            if (wireType == 2) {
                outComment.userId = QString::fromUtf8(readLengthDelimited(raw, size, offset));
                outComment.isAnonymous = true;
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        case 7: // modifier: Modifier
            if (wireType == 2) {
                QByteArray modBytes = readLengthDelimited(raw, size, offset);
                parseChatModifier(modBytes, outComment.modifier);
            } else {
                skipField(wireType, raw, size, offset);
            }
            break;
        default:
            skipField(wireType, raw, size, offset);
            break;
        }
    }

    if (outComment.date == 0) {
        outComment.date = QDateTime::currentSecsSinceEpoch();
    }

    return !outComment.text.isEmpty();
}

void NdgrDecoder::parseChatModifier(const QByteArray &data, CommentModifier &outMod)
{
    const char *raw = data.constData();
    int size = data.size();
    int offset = 0;

    static const QString colorNames[] = {
        "", "red", "pink", "orange", "yellow", "green", "cyan", "blue", "purple", "black",
        "white2", "red2", "pink2", "orange2", "yellow2", "green2", "cyan2", "blue2", "purple2", "black2"
    };

    while (offset < size) {
        quint64 key = readVarint(raw, size, offset);
        int fieldNumber = static_cast<int>(key >> 3);
        int wireType = static_cast<int>(key & 0x07);

        switch (fieldNumber) {
        case 1: // Pos (0=naka, 1=shita, 2=ue)
            if (wireType == 0) {
                quint64 pos = readVarint(raw, size, offset);
                if (pos == 1) outMod.position = "shita";
                else if (pos == 2) outMod.position = "ue";
                else outMod.position = "naka";
            } else skipField(wireType, raw, size, offset);
            break;
        case 2: // Size (0=medium, 1=small, 2=big)
            if (wireType == 0) {
                quint64 sz = readVarint(raw, size, offset);
                if (sz == 1) outMod.size = "small";
                else if (sz == 2) outMod.size = "big";
                else outMod.size = "medium";
            } else skipField(wireType, raw, size, offset);
            break;
        case 3: // named_color
            if (wireType == 0) {
                quint64 col = readVarint(raw, size, offset);
                if (col < sizeof(colorNames) / sizeof(colorNames[0])) {
                    outMod.color = colorNames[col];
                }
            } else skipField(wireType, raw, size, offset);
            break;
        case 4: // full_color: FullColor (r=1, g=2, b=3)
            if (wireType == 2) {
                QByteArray fcBytes = readLengthDelimited(raw, size, offset);
                int r = 0, g = 0, b = 0;
                int fcOffset = 0;
                while (fcOffset < fcBytes.size()) {
                    quint64 fcKey = readVarint(fcBytes.constData(), fcBytes.size(), fcOffset);
                    int fcField = static_cast<int>(fcKey >> 3);
                    int fcWire = static_cast<int>(fcKey & 0x07);
                    if (fcField == 1 && fcWire == 0) r = static_cast<int>(readVarint(fcBytes.constData(), fcBytes.size(), fcOffset));
                    else if (fcField == 2 && fcWire == 0) g = static_cast<int>(readVarint(fcBytes.constData(), fcBytes.size(), fcOffset));
                    else if (fcField == 3 && fcWire == 0) b = static_cast<int>(readVarint(fcBytes.constData(), fcBytes.size(), fcOffset));
                    else skipField(fcWire, fcBytes.constData(), fcBytes.size(), fcOffset);
                }
                outMod.color = QString("#%1%2%3")
                                   .arg(r, 2, 16, QChar('0'))
                                   .arg(g, 2, 16, QChar('0'))
                                   .arg(b, 2, 16, QChar('0'));
            } else skipField(wireType, raw, size, offset);
            break;
        case 5: // Font (0=defont, 1=mincho, 2=gothic)
            if (wireType == 0) {
                quint64 fnt = readVarint(raw, size, offset);
                if (fnt == 1) outMod.font = "mincho";
                else if (fnt == 2) outMod.font = "gothic";
                else outMod.font = "defont";
            } else skipField(wireType, raw, size, offset);
            break;
        default:
            skipField(wireType, raw, size, offset);
            break;
        }
    }
}

} // namespace Nicolive


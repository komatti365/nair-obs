#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QColor>

namespace Nicolive {

struct UserInfo {
    QString userId;
    QString nickname;
    bool isPremium = false;
    QString iconUrl;
    bool isValid() const { return !userId.isEmpty(); }
};

struct ProgramInfo {
    QString nicoliveProgramId; // e.g. "lv12345678"
    QString title;
    QString description;
    QString status; // "before_release", "released", "on_air", "ended"
    qint64 beginTime = 0;   // unix timestamp in seconds
    qint64 endTime = 0;     // unix timestamp in seconds
    qint64 vposBaseTime = 0;
    qint64 viewCount = 0;
    qint64 commentCount = 0;
    bool isMemberOnly = false;

    // Ingest stream info
    QString streamUrl;  // e.g. "rtmp://..."
    QString streamKey;  // e.g. "nicolive...."

    // Message server / WebSocket info
    QString viewUrl;
    QString wsUrl;

    bool isOnAir() const { return status == "on_air"; }
    bool isEnded() const { return status == "ended"; }
    bool isValid() const { return !nicoliveProgramId.isEmpty(); }
};

struct CommentModifier {
    QString color;      // hex color like "#FF0000" or empty
    QString position;   // "ue", "naka", "shita"
    QString size;       // "big", "medium", "small"
    QString font;       // "defont", "mincho", "gothic"
    bool isTranslucent = false;
};

struct CommentItem {
    QString id;
    qint64 no = 0;
    QString userId;
    QString userName;
    QString text;
    qint64 vpos = 0;
    qint64 date = 0;    // unix timestamp (seconds)
    qint64 dateUsec = 0;
    bool isPremium = false;
    bool isBroadcaster = false;
    bool isAnonymous = false; // 184
    QString mail;       // raw command string
    CommentModifier modifier;
    QColor displayColor = QColor(255, 255, 255);
};

} // namespace Nicolive


#include "NicoliveCommentReceiver.h"
#include "NdgrDecoder.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace Nicolive {

NicoliveCommentReceiver::NicoliveCommentReceiver(QObject *parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_keepSeatTimer(new QTimer(this))
    , m_ndgrHttp(new QNetworkAccessManager(this))
    , m_segmentPollTimer(new QTimer(this))
{
    connect(m_webSocket, &QWebSocket::connected, this, &NicoliveCommentReceiver::onWsConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &NicoliveCommentReceiver::onWsDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &NicoliveCommentReceiver::onWsTextMessageReceived);
    connect(m_webSocket, &QWebSocket::errorOccurred, this, &NicoliveCommentReceiver::onWsError);

    connect(m_keepSeatTimer, &QTimer::timeout, this, &NicoliveCommentReceiver::sendKeepSeat);

    m_segmentPollTimer->setInterval(1000);
    connect(m_segmentPollTimer, &QTimer::timeout, [this]() {
        if (!m_nextSegmentUri.isEmpty()) {
            fetchNdgrSegment(m_nextSegmentUri);
        }
    });
}

NicoliveCommentReceiver::~NicoliveCommentReceiver()
{
    stop();
}

void NicoliveCommentReceiver::start(const QString &userId, const QString &programId,
                                    const QString &wsUrl, const QString &viewUrl)
{
    stop();

    m_userId = userId;
    m_programId = programId;
    m_wsUrl = wsUrl;
    m_viewUrl = viewUrl;
    m_receivedMessageIds.clear();
    m_commentCounter = 0;

    if (!m_wsUrl.isEmpty()) {
        connectWebSocket(m_wsUrl);
    }
    if (!m_viewUrl.isEmpty()) {
        fetchNdgrView(m_viewUrl, "now");
    }
}

void NicoliveCommentReceiver::stop()
{
    m_keepSeatTimer->stop();
    m_segmentPollTimer->stop();

    if (m_webSocket->isValid()) {
        m_webSocket->close();
    }
    m_isConnected = false;
    m_nextSegmentUri.clear();
    emit connectionStateChanged(false);
}

void NicoliveCommentReceiver::connectWebSocket(const QString &url)
{
    qInfo() << "[N-Air OBS] Connecting WebSocket:" << url;
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");
    m_webSocket->open(request);
}

void NicoliveCommentReceiver::onWsConnected()
{
    qInfo() << "[N-Air OBS] WebSocket connected!";
    m_isConnected = true;
    emit connectionStateChanged(true);

    // Send startWatching
    QJsonObject data;
    QJsonObject stream;
    stream["quality"] = "high";
    stream["protocol"] = "hls";
    stream["latency"] = "low";
    stream["chasePlay"] = false;
    data["stream"] = stream;

    QJsonObject room;
    room["isAllowedToBeNext"] = true;
    data["room"] = room;
    data["reconnect"] = false;

    QJsonObject req;
    req["type"] = "startWatching";
    req["data"] = data;

    m_webSocket->sendTextMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
}

void NicoliveCommentReceiver::onWsDisconnected()
{
    qInfo() << "[N-Air OBS] WebSocket disconnected.";
    m_isConnected = false;
    m_keepSeatTimer->stop();
    emit connectionStateChanged(false);
}

void NicoliveCommentReceiver::onWsError(QAbstractSocket::SocketError error)
{
    qWarning() << "[N-Air OBS] WebSocket error:" << error << m_webSocket->errorString();
    emit receiverError(m_webSocket->errorString());
}

void NicoliveCommentReceiver::onWsTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    QJsonObject data = obj["data"].toObject();

    if (type == "seat") {
        int intervalSec = data["keepIntervalSec"].toInt(30);
        m_keepSeatTimer->start(intervalSec * 1000);
    } else if (type == "ping") {
        // Reply with pong
        QJsonObject pong;
        pong["type"] = "pong";
        m_webSocket->sendTextMessage(QJsonDocument(pong).toJson(QJsonDocument::Compact));
    } else if (type == "statistics") {
        qint64 viewers = data["viewers"].toInteger(0);
        qint64 comments = data["comments"].toInteger(0);
        emit statisticsUpdated(viewers, comments);
    } else if (type == "messageServer") {
        QString viewUrl = data["viewUrl"].toString();
        if (!viewUrl.isEmpty() && m_viewUrl != viewUrl) {
            m_viewUrl = viewUrl;
            fetchNdgrView(m_viewUrl, "now");
        }
    } else if (type == "postCommentResult") {
        // Comment posted response
    }
}

void NicoliveCommentReceiver::sendKeepSeat()
{
    if (m_webSocket->isValid()) {
        QJsonObject keepSeat;
        keepSeat["type"] = "keepSeat";
        m_webSocket->sendTextMessage(QJsonDocument(keepSeat).toJson(QJsonDocument::Compact));
    }
}

void NicoliveCommentReceiver::fetchNdgrView(const QString &viewUrl, const QString &at)
{
    QUrl url(QString("%1?at=%2").arg(viewUrl, at));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");

    QNetworkReply *reply = m_ndgrHttp->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[N-Air OBS] Failed to fetch NDGR view:" << reply->errorString();
            return;
        }

        QByteArray data = reply->readAll();
        // The view API returns protobuf ChunkedEntry or JSON segment references
        // In Re:NDGR view v4, if requested with at=now, it streams entries
        // Let's check if the response starts with JSON or protobuf
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isObject()) {
            QJsonObject root = jsonDoc.object();
            if (root.contains("data")) {
                // Parse view json if applicable
            }
        } else {
            // Protobuf chunked stream: extract segment uri or messages
            QList<CommentItem> items = NdgrDecoder::decodeChunkedMessages(data);
            for (auto &item : items) {
                if (item.id.isEmpty() || !m_receivedMessageIds.contains(item.id)) {
                    if (!item.id.isEmpty()) m_receivedMessageIds.insert(item.id);
                    item.no = ++m_commentCounter;
                    emit commentReceived(item);
                }
            }
        }
    });
}

void NicoliveCommentReceiver::fetchNdgrSegment(const QString &segmentUri)
{
    QNetworkRequest request{QUrl(segmentUri)};
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");

    QNetworkReply *reply = m_ndgrHttp->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }

        QByteArray data = reply->readAll();
        QList<CommentItem> items = NdgrDecoder::decodeChunkedMessages(data);
        for (auto &item : items) {
            if (item.id.isEmpty() || !m_receivedMessageIds.contains(item.id)) {
                if (!item.id.isEmpty()) m_receivedMessageIds.insert(item.id);
                item.no = ++m_commentCounter;
                emit commentReceived(item);
            }
        }
    });
}

} // namespace Nicolive


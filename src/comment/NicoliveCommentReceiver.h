#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QTimer>
#include "SimpleWebSocketClient.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../common/NicoliveData.h"

namespace Nicolive {

class NicoliveCommentReceiver : public QObject {
	Q_OBJECT

public:
	explicit NicoliveCommentReceiver(QObject *parent = nullptr);
	~NicoliveCommentReceiver() override;

	void start(const QString &userId, const QString &programId, const QString &wsUrl = QString(),
		   const QString &viewUrl = QString());
	void stop();

	bool isConnected() const { return m_isConnected; }
	const QString &currentProgramId() const { return m_programId; }

signals:
	void commentReceived(const CommentItem &comment);
	void statisticsUpdated(qint64 viewers, qint64 comments);
	void connectionStateChanged(bool connected);
	void receiverError(const QString &errorMsg);

private slots:
	void onWsConnected();
	void onWsDisconnected();
	void onWsTextMessageReceived(const QString &message);
	void onWsError(QAbstractSocket::SocketError error);
	void sendKeepSeat();

private:
	void connectWebSocket(const QString &url);
	void fetchNdgrView(const QString &viewUrl, const QString &at = "now");
	void fetchNdgrSegment(const QString &segmentUri);

	QString m_userId;
	QString m_programId;
	QString m_wsUrl;
	QString m_viewUrl;

	bool m_isConnected = false;
	SimpleWebSocketClient *m_webSocket = nullptr;
	QTimer *m_keepSeatTimer = nullptr;

	QNetworkAccessManager *m_ndgrHttp = nullptr;
	QTimer *m_segmentPollTimer = nullptr;
	QString m_nextSegmentUri;
	QSet<QString> m_receivedMessageIds;
	qint64 m_commentCounter = 0;
};

} // namespace Nicolive

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <functional>
#include "../common/NicoliveData.h"

namespace Nicolive {

class NicoliveApiClient : public QObject {
	Q_OBJECT

public:
	explicit NicoliveApiClient(QObject *parent = nullptr);
	~NicoliveApiClient() override = default;

	// Latest user program (GET /api/v1/unama/programs/latest)
	void fetchLatestProgram(
		const QString &userId,
		std::function<void(bool success, const ProgramInfo &info, const QString &errorMsg)> callback);

	// Specific program info (GET /api/v1/unama/programs)
	void fetchProgram(const QString &userId, const QString &programId,
			  std::function<void(bool success, const ProgramInfo &info, const QString &errorMsg)> callback);

	// Stream info: RTMP URL & stream key (GET /api/v1/unama/programs/broadcast_stream)
	void fetchBroadcastStream(
		const QString &userId, const QString &programId,
		std::function<void(bool success, const QString &url, const QString &key, const QString &errorMsg)>
			callback);

	// Program controls
	void startProgram(const QString &userId, const QString &programId,
			  std::function<void(bool success, const QString &errorMsg)> callback);

	void endProgram(const QString &userId, const QString &programId,
			std::function<void(bool success, const QString &errorMsg)> callback);

	void extendProgram(const QString &userId, const QString &programId, int minutes,
			   std::function<void(bool success, const QString &errorMsg)> callback);

	// Broadcaster comment (PUT /api/v1/unama/programs/broadcaster_comment)
	void sendBroadcasterComment(const QString &userId, const QString &programId, const QString &text,
				    const QString &userName = QString(), const QString &color = QString(),
				    bool isPermanent = false,
				    std::function<void(bool success, const QString &errorMsg)> callback = nullptr);

	// Normal comment (POST /unama/tool/v2/programs/{programId}/comments)
	void sendNormalComment(const QString &programId, const QString &text, qint64 vpos = 0,
			       std::function<void(bool success, const QString &errorMsg)> callback = nullptr);

	// Rooms / Message server info (GET /api/v1/unama/programs/rooms)
	void fetchRooms(const QString &userId, const QString &programId,
			std::function<void(bool success, const QString &viewUrl, const QString &errorMsg)> callback);

	// WebSocket endpoint (GET /api/v1/wsendpoint)
	void fetchWsEndpoint(const QString &userId, const QString &programId,
			     std::function<void(bool success, const QString &wsUrl, const QString &errorMsg)> callback);

private:
	QNetworkRequest createRequest(const QUrl &url, bool requiresAuth = true);
	void sendJsonRequest(
		const QString &method, const QUrl &url, const QJsonObject &body,
		std::function<void(bool success, const QJsonObject &responseObj, const QString &errorMsg)> callback);

	QNetworkAccessManager m_networkManager;
};

} // namespace Nicolive

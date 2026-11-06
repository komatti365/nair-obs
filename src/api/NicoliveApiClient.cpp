#include "NicoliveApiClient.h"
#include "../auth/NicoliveAuthManager.h"
#include "../common/ConfigManager.h"
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace Nicolive {

static const QString LIVE2_API_BASE = "https://api.live2.nicovideo.jp";

NicoliveApiClient::NicoliveApiClient(QObject *parent) : QObject(parent) {}

QNetworkRequest NicoliveApiClient::createRequest(const QUrl &url, bool requiresAuth)
{
	QNetworkRequest request(url);
	request.setRawHeader("User-Agent", "N-Air-OBS/1.0");
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	if (requiresAuth) {
		QString token = NicoliveAuthManager::instance().accessToken();
		if (!token.isEmpty()) {
			request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
		}
		QString session = ConfigManager::instance().userSession();
		if (!session.isEmpty()) {
			request.setRawHeader("Cookie", QString("user_session=%1").arg(session).toUtf8());
		}
	}
	return request;
}

void NicoliveApiClient::sendJsonRequest(
	const QString &method, const QUrl &url, const QJsonObject &body,
	std::function<void(bool success, const QJsonObject &responseObj, const QString &errorMsg)> callback)
{
	QNetworkRequest request = createRequest(url);
	QByteArray postData = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);

	QNetworkReply *reply = nullptr;
	if (method == "GET") {
		reply = m_networkManager.get(request);
	} else if (method == "POST") {
		reply = m_networkManager.post(request, postData);
	} else if (method == "PUT") {
		reply = m_networkManager.put(request, postData);
	} else if (method == "DELETE") {
		reply = m_networkManager.deleteResource(request);
	}

	if (!reply)
		return;

	connect(reply, &QNetworkReply::finished, [reply, callback]() {
		reply->deleteLater();
		int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray data = reply->readAll();

		QJsonDocument doc = QJsonDocument::fromJson(data);
		QJsonObject root = doc.isObject() ? doc.object() : QJsonObject();

		if (reply->error() != QNetworkReply::NoError && (httpCode < 200 || httpCode >= 300)) {
			QString errorMsg = reply->errorString();
			if (root.contains("meta")) {
				QJsonObject meta = root["meta"].toObject();
				if (meta.contains("errorCode")) {
					errorMsg = QString("[%1] %2").arg(meta["errorCode"].toString(),
									  meta["errorMessage"].toString());
				}
			}
			if (callback)
				callback(false, root, errorMsg);
			return;
		}

		if (callback)
			callback(true, root, QString());
	});
}

void NicoliveApiClient::fetchLatestProgram(
	const QString &userId,
	std::function<void(bool success, const ProgramInfo &info, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/latest");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	url.setQuery(query);

	sendJsonRequest("GET", url, QJsonObject(), [callback](bool success, const QJsonObject &obj, const QString &err) {
		if (!success) {
			if (callback)
				callback(false, ProgramInfo(), err);
			return;
		}

		QJsonObject data = obj["data"].toObject();
		ProgramInfo info;
		info.nicoliveProgramId = data["nicoliveProgramId"].toString();
		info.title = data["title"].toString();
		info.description = data["description"].toString();
		info.status = data["status"].toString();
		info.isMemberOnly = data["isMemberOnly"].toBool();

		if (data.contains("beginTime")) {
			info.beginTime =
				QDateTime::fromString(data["beginTime"].toString(), Qt::ISODate).toSecsSinceEpoch();
		}
		if (data.contains("endTime")) {
			info.endTime =
				QDateTime::fromString(data["endTime"].toString(), Qt::ISODate).toSecsSinceEpoch();
		}
		if (data.contains("vposBaseTime")) {
			info.vposBaseTime =
				QDateTime::fromString(data["vposBaseTime"].toString(), Qt::ISODate).toSecsSinceEpoch();
		}

		if (callback)
			callback(true, info, QString());
	});
}

void NicoliveApiClient::fetchProgram(
	const QString &userId, const QString &programId,
	std::function<void(bool success, const ProgramInfo &info, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	sendJsonRequest("GET", url, QJsonObject(),
			[callback, programId](bool success, const QJsonObject &obj, const QString &err) {
				if (!success) {
					if (callback)
						callback(false, ProgramInfo(), err);
					return;
				}

				QJsonObject data = obj["data"].toObject();
				ProgramInfo info;
				info.nicoliveProgramId = programId;
				info.title = data["title"].toString();
				info.description = data["description"].toString();
				info.status = data["status"].toString();
				info.isMemberOnly = data["isMemberOnly"].toBool();

				if (data.contains("beginTime")) {
					info.beginTime =
						QDateTime::fromString(data["beginTime"].toString(), Qt::ISODate)
							.toSecsSinceEpoch();
				}
				if (data.contains("endTime")) {
					info.endTime = QDateTime::fromString(data["endTime"].toString(), Qt::ISODate)
							       .toSecsSinceEpoch();
				}

				if (callback)
					callback(true, info, QString());
			});
}

void NicoliveApiClient::fetchBroadcastStream(
	const QString &userId, const QString &programId,
	std::function<void(bool success, const QString &url, const QString &key, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/broadcast_stream");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	if (!programId.isEmpty()) {
		query.addQueryItem("nicoliveProgramId", programId);
	}
	url.setQuery(query);

	sendJsonRequest("GET", url, QJsonObject(),
			[callback](bool success, const QJsonObject &obj, const QString &err) {
				if (!success) {
					if (callback)
						callback(false, QString(), QString(), err);
					return;
				}

				QJsonObject data = obj["data"].toObject();
				QString rtmpUrl = data["url"].toString();
				QString streamName = data["name"].toString();

				if (callback)
					callback(true, rtmpUrl, streamName, QString());
			});
}

void NicoliveApiClient::startProgram(const QString &userId, const QString &programId,
				     std::function<void(bool success, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/start");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	sendJsonRequest("PUT", url, QJsonObject(), [callback](bool success, const QJsonObject &, const QString &err) {
		if (callback)
			callback(success, err);
	});
}

void NicoliveApiClient::endProgram(const QString &userId, const QString &programId,
				   std::function<void(bool success, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/end");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	sendJsonRequest("PUT", url, QJsonObject(), [callback](bool success, const QJsonObject &, const QString &err) {
		if (callback)
			callback(success, err);
	});
}

void NicoliveApiClient::extendProgram(const QString &userId, const QString &programId, int minutes,
				      std::function<void(bool success, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/extension");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	QJsonObject body;
	body["minutes"] = minutes;

	sendJsonRequest("PUT", url, body, [callback](bool success, const QJsonObject &, const QString &err) {
		if (callback)
			callback(success, err);
	});
}

void NicoliveApiClient::sendBroadcasterComment(const QString &userId, const QString &programId, const QString &text,
					       const QString &userName, const QString &color, bool isPermanent,
					       std::function<void(bool success, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/broadcaster_comment");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	QJsonObject body;
	body["text"] = text;
	if (!userName.isEmpty())
		body["userName"] = userName;
	if (!color.isEmpty())
		body["color"] = color;
	body["isPermanent"] = isPermanent;

	sendJsonRequest("PUT", url, body, [callback](bool success, const QJsonObject &, const QString &err) {
		if (callback)
			callback(success, err);
	});
}

void NicoliveApiClient::sendNormalComment(const QString &programId, const QString &text, qint64 vpos,
					  std::function<void(bool success, const QString &errorMsg)> callback)
{
	QUrl url(QString("%1/unama/tool/v2/programs/%2/comments").arg(LIVE2_API_BASE, programId));

	QJsonObject body;
	body["text"] = text;
	body["vpos"] = vpos;

	sendJsonRequest("POST", url, body, [callback](bool success, const QJsonObject &, const QString &err) {
		if (callback)
			callback(success, err);
	});
}

void NicoliveApiClient::fetchRooms(
	const QString &userId, const QString &programId,
	std::function<void(bool success, const QString &viewUrl, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/unama/programs/rooms");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	sendJsonRequest("GET", url, QJsonObject(),
			[callback](bool success, const QJsonObject &obj, const QString &err) {
				if (!success) {
					if (callback)
						callback(false, QString(), err);
					return;
				}

				QJsonArray data = obj["data"].toArray();
				QString viewUrl;
				if (!data.isEmpty()) {
					QJsonObject room = data[0].toObject();
					viewUrl = room["viewUrl"].toString();
				}

				if (callback)
					callback(true, viewUrl, QString());
			});
}

void NicoliveApiClient::fetchWsEndpoint(
	const QString &userId, const QString &programId,
	std::function<void(bool success, const QString &wsUrl, const QString &errorMsg)> callback)
{
	QUrl url(LIVE2_API_BASE + "/api/v1/wsendpoint");
	QUrlQuery query;
	query.addQueryItem("userId", userId);
	query.addQueryItem("nicoliveProgramId", programId);
	url.setQuery(query);

	sendJsonRequest("GET", url, QJsonObject(),
			[callback](bool success, const QJsonObject &obj, const QString &err) {
				if (!success) {
					if (callback)
						callback(false, QString(), err);
					return;
				}

				QJsonObject data = obj["data"].toObject();
				QString wsUrl = data["url"].toString();

				if (callback)
					callback(true, wsUrl, QString());
			});
}

} // namespace Nicolive

#include "NicoliveAuthManager.h"
#include "../common/ConfigManager.h"
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace Nicolive {

static const QString DEFAULT_CLIENT_ID = ""; // User can configure Client ID in settings

NicoliveAuthManager &NicoliveAuthManager::instance()
{
    static NicoliveAuthManager inst;
    return inst;
}

NicoliveAuthManager::NicoliveAuthManager()
    : QObject(nullptr)
    , m_authServer(new QTcpServer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_authServer, &QTcpServer::newConnection, this, &NicoliveAuthManager::handleNewConnection);

    // Try restoring previous session
    QString token = ConfigManager::instance().accessToken();
    if (!token.isEmpty()) {
        fetchUserProfile();
    }
}

NicoliveAuthManager::~NicoliveAuthManager()
{
    cancelOAuthLogin();
}

QString NicoliveAuthManager::accessToken() const
{
    return ConfigManager::instance().accessToken();
}

void NicoliveAuthManager::startOAuthLogin(const QString &clientId, const QString &clientSecret)
{
    cancelOAuthLogin();

    m_currentClientId = clientId.isEmpty() ? ConfigManager::instance().clientId() : clientId;
    m_currentClientSecret = clientSecret.isEmpty() ? ConfigManager::instance().clientSecret() : clientSecret;

    if (m_currentClientId.isEmpty()) {
        emit loginError(tr("Client ID is empty. Please configure Client ID in settings."));
        return;
    }

    if (!m_authServer->listen(QHostAddress::LocalHost, m_authPort)) {
        // Try next port if busy
        if (!m_authServer->listen(QHostAddress::LocalHost, 0)) {
            emit loginError(tr("Failed to start local authorization listener server."));
            return;
        }
    }

    quint16 port = m_authServer->serverPort();
    QString redirectUri = QString("http://127.0.0.1:%1/callback").arg(port);

    QUrl authUrl("https://oauth.nicovideo.jp/oauth2/authorize");
    QUrlQuery query;
    query.addQueryItem("client_id", m_currentClientId);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", redirectUri);
    query.addQueryItem("scope", "user.authorities.relives.broadcast user.authorities.relives.watch.get user.authorities.relives.watch.interact");
    authUrl.setQuery(query);

    QDesktopServices::openUrl(authUrl);
    emit oauthWaitingForAuthorization();
}

void NicoliveAuthManager::cancelOAuthLogin()
{
    if (m_authServer->isListening()) {
        m_authServer->close();
    }
}

void NicoliveAuthManager::handleNewConnection()
{
    QTcpSocket *socket = m_authServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, [this, socket]() {
        QByteArray requestData = socket->readAll();
        QString request = QString::fromUtf8(requestData);

        // Parse GET /callback?code=... HTTP/1.1
        QRegularExpression regex(R"(GET\s+/callback\?([^ ]+)\s+HTTP)");
        QRegularExpressionMatch match = regex.match(request);

        QString code;
        if (match.hasMatch()) {
            QUrlQuery query(match.captured(1));
            code = query.queryItemValue("code");
        }

        QString responseBody;
        if (!code.isEmpty()) {
            responseBody = "<html><body><h2>N-Air OBS: Authentication Successful!</h2><p>You can close this window now.</p></body></html>";
        } else {
            responseBody = "<html><body><h2>N-Air OBS: Authentication Failed</h2><p>Could not retrieve authorization code.</p></body></html>";
        }

        QByteArray httpResponse;
        httpResponse.append("HTTP/1.1 200 OK\r\n");
        httpResponse.append("Content-Type: text/html; charset=utf-8\r\n");
        httpResponse.append(QString("Content-Length: %1\r\n").arg(responseBody.toUtf8().size()).toUtf8());
        httpResponse.append("Connection: close\r\n\r\n");
        httpResponse.append(responseBody.toUtf8());

        socket->write(httpResponse);
        socket->flush();
        socket->disconnectFromHost();

        m_authServer->close();

        if (!code.isEmpty()) {
            exchangeCodeForToken(code);
        } else {
            emit loginError(tr("Authorization failed: Code parameter missing."));
        }
    });
}

void NicoliveAuthManager::exchangeCodeForToken(const QString &code)
{
    quint16 port = m_authServer->serverPort();
    QString redirectUri = QString("http://127.0.0.1:%1/callback").arg(port);

    QUrl tokenUrl("https://oauth.nicovideo.jp/oauth2/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");

    QUrlQuery params;
    params.addQueryItem("grant_type", "authorization_code");
    params.addQueryItem("client_id", m_currentClientId);
    params.addQueryItem("client_secret", m_currentClientSecret);
    params.addQueryItem("code", code);
    params.addQueryItem("redirect_uri", redirectUri);

    QByteArray postData = params.query(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = m_networkManager->post(request, postData);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginError(tr("Token exchange failed: %1").arg(reply->errorString()));
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit loginError(tr("Invalid token response format."));
            return;
        }

        QJsonObject obj = doc.object();
        QString accessToken = obj["access_token"].toString();
        QString refreshToken = obj["refresh_token"].toString();

        if (accessToken.isEmpty()) {
            emit loginError(tr("No access token returned from authorization server."));
            return;
        }

        ConfigManager::instance().setAccessToken(accessToken);
        ConfigManager::instance().setRefreshToken(refreshToken);
        ConfigManager::instance().setClientId(m_currentClientId);
        ConfigManager::instance().setClientSecret(m_currentClientSecret);
        ConfigManager::instance().save();

        fetchUserProfile();
    });
}

void NicoliveAuthManager::loginWithToken(const QString &token)
{
    if (token.isEmpty()) return;
    ConfigManager::instance().setAccessToken(token);
    ConfigManager::instance().save();
    fetchUserProfile();
}

void NicoliveAuthManager::loginWithUserSession(const QString &userSession)
{
    if (userSession.isEmpty()) return;
    ConfigManager::instance().setUserSession(userSession);
    ConfigManager::instance().save();
    fetchUserProfile();
}

void NicoliveAuthManager::logout()
{
    ConfigManager::instance().setAccessToken("");
    ConfigManager::instance().setRefreshToken("");
    ConfigManager::instance().setUserSession("");
    ConfigManager::instance().save();

    m_userInfo = UserInfo();
    emit authStatusChanged(false);
    emit userInfoUpdated(m_userInfo);
}

void NicoliveAuthManager::refreshUserInfo()
{
    if (!accessToken().isEmpty() || !ConfigManager::instance().userSession().isEmpty()) {
        fetchUserProfile();
    }
}

void NicoliveAuthManager::fetchUserProfile()
{
    // Try GET /api/v1/user/nickname
    QUrl url("https://api.live2.nicovideo.jp/api/v1/user/nickname");
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");

    QString token = accessToken();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    }
    QString userSession = ConfigManager::instance().userSession();
    if (!userSession.isEmpty()) {
        request.setRawHeader("Cookie", QString("user_session=%1").arg(userSession).toUtf8());
    }

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[N-Air OBS] Failed to fetch user nickname:" << reply->errorString();
            emit authStatusChanged(false);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit authStatusChanged(false);
            return;
        }

        QJsonObject root = doc.object();
        QJsonObject dataObj = root["data"].toObject();

        QString nickname = dataObj["nickname"].toString();
        qint64 userIdNum = dataObj["userId"].toInteger(0);
        QString userId = QString::number(userIdNum);

        if (userIdNum > 0) {
            m_userInfo.userId = userId;
            m_userInfo.nickname = nickname;

            // Generate user icon URL
            qint64 dir = userIdNum / 10000;
            m_userInfo.iconUrl = QString("https://secure-dcdn.cdn.nimg.jp/nicoaccount/usericon/%1/%2.jpg")
                                     .arg(dir)
                                     .arg(userIdNum);

            ConfigManager::instance().setLastUserId(userId);
            ConfigManager::instance().save();

            fetchPremiumStatus();

            emit authStatusChanged(true);
            emit userInfoUpdated(m_userInfo);
        } else {
            emit authStatusChanged(false);
        }
    });
}

void NicoliveAuthManager::fetchPremiumStatus()
{
    QString token = accessToken();
    if (token.isEmpty()) return;

    QUrl url("https://oauth.nicovideo.jp/v1/user/premium.json");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    request.setRawHeader("User-Agent", "N-Air-OBS/1.0");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonObject dataObj = root["data"].toObject();
                m_userInfo.isPremium = (dataObj["type"].toString() == "premium");
                emit userInfoUpdated(m_userInfo);
            }
        }
    });
}

} // namespace Nicolive


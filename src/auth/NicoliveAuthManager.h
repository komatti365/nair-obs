#pragma once

#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../common/NicoliveData.h"

namespace Nicolive {

class NicoliveAuthManager : public QObject {
    Q_OBJECT

public:
    static NicoliveAuthManager &instance();

    bool isLoggedIn() const { return m_userInfo.isValid(); }
    const UserInfo &userInfo() const { return m_userInfo; }
    QString accessToken() const;

    void startOAuthLogin(const QString &clientId = QString(), const QString &clientSecret = QString());
    void cancelOAuthLogin();

    void loginWithToken(const QString &token);
    void loginWithUserSession(const QString &userSession);
    void logout();

    void refreshUserInfo();

signals:
    void authStatusChanged(bool isLoggedIn);
    void userInfoUpdated(const UserInfo &info);
    void loginError(const QString &message);
    void oauthWaitingForAuthorization();

private slots:
    void handleNewConnection();

private:
    NicoliveAuthManager();
    ~NicoliveAuthManager() override;
    NicoliveAuthManager(const NicoliveAuthManager &) = delete;
    NicoliveAuthManager &operator=(const NicoliveAuthManager &) = delete;

    void exchangeCodeForToken(const QString &code);
    void fetchUserProfile();
    void fetchPremiumStatus();

    UserInfo m_userInfo;
    QTcpServer *m_authServer = nullptr;
    quint16 m_authPort = 8765;
    QNetworkAccessManager *m_networkManager = nullptr;
    QString m_currentClientId;
    QString m_currentClientSecret;
};

} // namespace Nicolive


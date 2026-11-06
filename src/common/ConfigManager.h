#pragma once

#include <QString>
#include <QObject>
#include <obs-module.h>

namespace Nicolive {

class ConfigManager : public QObject {
    Q_OBJECT

public:
    static ConfigManager &instance();

    void load();
    void save();

    // OAuth & Auth tokens
    QString accessToken() const { return m_accessToken; }
    void setAccessToken(const QString &token) { m_accessToken = token; }

    QString refreshToken() const { return m_refreshToken; }
    void setRefreshToken(const QString &token) { m_refreshToken = token; }

    QString clientId() const { return m_clientId; }
    void setClientId(const QString &id) { m_clientId = id; }

    QString clientSecret() const { return m_clientSecret; }
    void setClientSecret(const QString &secret) { m_clientSecret = secret; }

    QString userSession() const { return m_userSession; }
    void setUserSession(const QString &session) { m_userSession = session; }

    QString lastUserId() const { return m_lastUserId; }
    void setLastUserId(const QString &userId) { m_lastUserId = userId; }

    // Automation flags
    bool autoApplyStreamKey() const { return m_autoApplyStreamKey; }
    void setAutoApplyStreamKey(bool enable) { m_autoApplyStreamKey = enable; }

    bool autoStartProgram() const { return m_autoStartProgram; }
    void setAutoStartProgram(bool enable) { m_autoStartProgram = enable; }

    bool autoEndProgram() const { return m_autoEndProgram; }
    void setAutoEndProgram(bool enable) { m_autoEndProgram = enable; }

    // Comment view settings
    int maxComments() const { return m_maxComments; }
    void setMaxComments(int count) { m_maxComments = count; }

    int fontSize() const { return m_fontSize; }
    void setFontSize(int size) { m_fontSize = size; }

    bool showAnonymous() const { return m_showAnonymous; }
    void setShowAnonymous(bool show) { m_showAnonymous = show; }

signals:
    void configChanged();

private:
    ConfigManager();
    ~ConfigManager() override = default;
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

    QString getConfigFilePath() const;

    QString m_accessToken;
    QString m_refreshToken;
    QString m_clientId;
    QString m_clientSecret;
    QString m_userSession;
    QString m_lastUserId;

    bool m_autoApplyStreamKey = true;
    bool m_autoStartProgram = false;
    bool m_autoEndProgram = false;

    int m_maxComments = 500;
    int m_fontSize = 12;
    bool m_showAnonymous = true;
};

} // namespace Nicolive


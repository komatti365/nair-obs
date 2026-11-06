#include "ConfigManager.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace Nicolive {

ConfigManager &ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager()
    : QObject(nullptr)
{
    load();
}

QString ConfigManager::getConfigFilePath() const
{
    char *config_dir = obs_module_config_path("");
    QString path;
    if (config_dir) {
        path = QString::fromUtf8(config_dir);
        bfree(config_dir);
    }
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QDir().mkpath(path);
    return QDir(path).filePath("nair-obs.json");
}

void ConfigManager::load()
{
    QString filePath = getConfigFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    m_accessToken = obj["accessToken"].toString();
    m_refreshToken = obj["refreshToken"].toString();
    m_clientId = obj["clientId"].toString();
    m_clientSecret = obj["clientSecret"].toString();
    m_userSession = obj["userSession"].toString();
    m_lastUserId = obj["lastUserId"].toString();

    if (obj.contains("autoApplyStreamKey"))
        m_autoApplyStreamKey = obj["autoApplyStreamKey"].toBool(true);
    if (obj.contains("autoStartProgram"))
        m_autoStartProgram = obj["autoStartProgram"].toBool(false);
    if (obj.contains("autoEndProgram"))
        m_autoEndProgram = obj["autoEndProgram"].toBool(false);

    if (obj.contains("maxComments"))
        m_maxComments = obj["maxComments"].toInt(500);
    if (obj.contains("fontSize"))
        m_fontSize = obj["fontSize"].toInt(12);
    if (obj.contains("showAnonymous"))
        m_showAnonymous = obj["showAnonymous"].toBool(true);
}

void ConfigManager::save()
{
    QString filePath = getConfigFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonObject obj;
    obj["accessToken"] = m_accessToken;
    obj["refreshToken"] = m_refreshToken;
    obj["clientId"] = m_clientId;
    obj["clientSecret"] = m_clientSecret;
    obj["userSession"] = m_userSession;
    obj["lastUserId"] = m_lastUserId;

    obj["autoApplyStreamKey"] = m_autoApplyStreamKey;
    obj["autoStartProgram"] = m_autoStartProgram;
    obj["autoEndProgram"] = m_autoEndProgram;

    obj["maxComments"] = m_maxComments;
    obj["fontSize"] = m_fontSize;
    obj["showAnonymous"] = m_showAnonymous;

    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();

    emit configChanged();
}

} // namespace Nicolive


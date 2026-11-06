#include "ObsStreamHelper.h"
#include <obs-frontend-api.h>
#include <obs.h>
#include <QDebug>

namespace Nicolive {

bool ObsStreamHelper::applyStreamSettings(const QString &streamUrl, const QString &streamKey)
{
    obs_service_t *service = obs_frontend_get_streaming_service();
    if (!service) {
        qWarning() << "[N-Air OBS] Failed to get streaming service from OBS.";
        return false;
    }

    obs_data_t *settings = obs_service_get_settings(service);
    if (!settings) {
        settings = obs_data_create();
    }

    // Set custom RTMP server and stream key
    obs_data_set_string(settings, "server", streamUrl.toUtf8().constData());
    obs_data_set_string(settings, "key", streamKey.toUtf8().constData());

    obs_service_update(service, settings);
    obs_data_release(settings);

    obs_frontend_save_streaming_service();

    qInfo() << "[N-Air OBS] Successfully applied stream settings (URL:" << streamUrl << ")";
    return true;
}

bool ObsStreamHelper::getCurrentStreamSettings(QString &streamUrl, QString &streamKey)
{
    obs_service_t *service = obs_frontend_get_streaming_service();
    if (!service) {
        return false;
    }

    obs_data_t *settings = obs_service_get_settings(service);
    if (!settings) {
        return false;
    }

    const char *server = obs_data_get_string(settings, "server");
    const char *key = obs_data_get_string(settings, "key");

    if (server) streamUrl = QString::fromUtf8(server);
    if (key) streamKey = QString::fromUtf8(key);

    obs_data_release(settings);
    return true;
}

bool ObsStreamHelper::isStreamingActive()
{
    return obs_frontend_streaming_active();
}

void ObsStreamHelper::startStreaming()
{
    if (!obs_frontend_streaming_active()) {
        obs_frontend_streaming_start();
    }
}

void ObsStreamHelper::stopStreaming()
{
    if (obs_frontend_streaming_active()) {
        obs_frontend_streaming_stop();
    }
}

} // namespace Nicolive


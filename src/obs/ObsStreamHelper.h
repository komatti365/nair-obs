#pragma once

#include <QString>
#include <QObject>

namespace Nicolive {

class ObsStreamHelper : public QObject {
	Q_OBJECT

public:
	static bool applyStreamSettings(const QString &streamUrl, const QString &streamKey);
	static bool getCurrentStreamSettings(QString &streamUrl, QString &streamKey);
	static bool isStreamingActive();
	static void startStreaming();
	static void stopStreaming();
};

} // namespace Nicolive

#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QUrl>
#include <QSslSocket>
#include <QSslError>
#include <QCryptographicHash>
#include <QRandomGenerator>

namespace Nicolive {

class SimpleWebSocketClient : public QObject {
	Q_OBJECT

public:
	explicit SimpleWebSocketClient(QObject *parent = nullptr);
	~SimpleWebSocketClient() override;

	void open(const QUrl &url);
	void close();
	bool isValid() const;
	bool isConnected() const { return m_handshakeCompleted; }
	qint64 sendTextMessage(const QString &message);
	QString errorString() const;

signals:
	void connected();
	void disconnected();
	void textMessageReceived(const QString &message);
	void errorOccurred(QAbstractSocket::SocketError error);

private slots:
	void onSocketConnected();
	void onSocketEncrypted();
	void onSocketReadyRead();
	void onSocketDisconnected();
	void onSocketError(QAbstractSocket::SocketError error);

private:
	void sendHandshake();
	void processIncomingData();
	bool processHandshakeResponse();
	void processWebSocketFrames();
	void sendFrame(quint8 opcode, const QByteArray &payload);

	QSslSocket *m_socket = nullptr;
	QUrl m_url;
	QByteArray m_handshakeKey;
	bool m_handshakeCompleted = false;
	QByteArray m_readBuffer;
	QByteArray m_currentTextMessage;
};

} // namespace Nicolive

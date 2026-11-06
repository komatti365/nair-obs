#include "SimpleWebSocketClient.h"
#include <QDebug>
#include <QtGlobal>

namespace Nicolive {

SimpleWebSocketClient::SimpleWebSocketClient(QObject *parent) : QObject(parent), m_socket(new QSslSocket(this))
{
	connect(m_socket, &QSslSocket::connected, this, &SimpleWebSocketClient::onSocketConnected);
	connect(m_socket, &QSslSocket::encrypted, this, &SimpleWebSocketClient::onSocketEncrypted);
	connect(m_socket, &QSslSocket::readyRead, this, &SimpleWebSocketClient::onSocketReadyRead);
	connect(m_socket, &QSslSocket::disconnected, this, &SimpleWebSocketClient::onSocketDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	connect(m_socket, &QAbstractSocket::errorOccurred, this, &SimpleWebSocketClient::onSocketError);
#else
	connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this,
		&SimpleWebSocketClient::onSocketError);
#endif

	connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
		for (const auto &err : errors) {
			qWarning() << "[SimpleWebSocket] SSL Error:" << err.errorString();
		}
		m_socket->ignoreSslErrors();
	});
}

SimpleWebSocketClient::~SimpleWebSocketClient()
{
	close();
}

void SimpleWebSocketClient::open(const QUrl &url)
{
	close();
	m_url = url;
	m_handshakeCompleted = false;
	m_readBuffer.clear();
	m_currentTextMessage.clear();

	bool isSsl = (url.scheme().toLower() == "wss");
	int port = url.port(isSsl ? 443 : 80);

	if (isSsl) {
		m_socket->connectToHostEncrypted(url.host(), static_cast<quint16>(port));
	} else {
		m_socket->connectToHost(url.host(), static_cast<quint16>(port));
	}
}

void SimpleWebSocketClient::close()
{
	m_handshakeCompleted = false;
	if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
		if (m_socket->state() == QAbstractSocket::ConnectedState) {
			sendFrame(0x8, QByteArray());
		}
		m_socket->disconnectFromHost();
	}
	m_readBuffer.clear();
	m_currentTextMessage.clear();
}

bool SimpleWebSocketClient::isValid() const
{
	return m_socket && (m_socket->state() == QAbstractSocket::ConnectedState) && m_handshakeCompleted;
}

QString SimpleWebSocketClient::errorString() const
{
	return m_socket ? m_socket->errorString() : QString();
}

void SimpleWebSocketClient::onSocketConnected()
{
	if (m_url.scheme().toLower() != "wss") {
		sendHandshake();
	}
}

void SimpleWebSocketClient::onSocketEncrypted()
{
	if (m_url.scheme().toLower() == "wss") {
		sendHandshake();
	}
}

void SimpleWebSocketClient::onSocketReadyRead()
{
	m_readBuffer.append(m_socket->readAll());
	processIncomingData();
}

void SimpleWebSocketClient::processIncomingData()
{
	if (!m_handshakeCompleted) {
		if (!processHandshakeResponse()) {
			return;
		}
	}
	if (m_handshakeCompleted) {
		processWebSocketFrames();
	}
}

void SimpleWebSocketClient::onSocketDisconnected()
{
	m_handshakeCompleted = false;
	emit disconnected();
}

void SimpleWebSocketClient::onSocketError(QAbstractSocket::SocketError error)
{
	emit errorOccurred(error);
}

void SimpleWebSocketClient::sendHandshake()
{
	QByteArray rawKey(16, 0);
	for (int i = 0; i < 16; ++i) {
		rawKey[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
	}
	m_handshakeKey = rawKey.toBase64();

	QString path = m_url.path();
	if (path.isEmpty()) {
		path = "/";
	}
	if (m_url.hasQuery()) {
		path += "?" + m_url.query();
	}

	QString req = QString("GET %1 HTTP/1.1\r\n"
			      "Host: %2\r\n"
			      "Upgrade: websocket\r\n"
			      "Connection: Upgrade\r\n"
			      "Sec-WebSocket-Key: %3\r\n"
			      "Sec-WebSocket-Version: 13\r\n"
			      "User-Agent: N-Air-OBS/1.0\r\n\r\n")
			      .arg(path, m_url.host(), QString::fromLatin1(m_handshakeKey));

	m_socket->write(req.toUtf8());
}

bool SimpleWebSocketClient::processHandshakeResponse()
{
	int headerEnd = static_cast<int>(m_readBuffer.indexOf("\r\n\r\n"));
	if (headerEnd == -1) {
		return false;
	}

	QByteArray headerData = m_readBuffer.left(headerEnd);
	m_readBuffer.remove(0, headerEnd + 4);

	int firstLineEnd = static_cast<int>(headerData.indexOf("\r\n"));
	QByteArray statusLine = (firstLineEnd != -1) ? headerData.left(firstLineEnd) : headerData;

	if (!statusLine.contains("101")) {
		qWarning() << "[SimpleWebSocket] Handshake failed, status line:" << statusLine;
		m_socket->disconnectFromHost();
		return false;
	}

	m_handshakeCompleted = true;
	emit connected();
	return true;
}

void SimpleWebSocketClient::processWebSocketFrames()
{
	while (m_readBuffer.size() >= 2) {
		const auto *data = reinterpret_cast<const quint8 *>(m_readBuffer.constData());
		quint8 b0 = data[0];
		quint8 b1 = data[1];

		bool fin = (b0 & 0x80) != 0;
		quint8 opcode = static_cast<quint8>(b0 & 0x0F);
		bool masked = (b1 & 0x80) != 0;
		quint64 payloadLen = static_cast<quint64>(b1 & 0x7F);

		qsizetype headerSize = 2;
		if (payloadLen == 126) {
			if (m_readBuffer.size() < headerSize + 2)
				return;
			payloadLen = (static_cast<quint64>(data[2]) << 8) | static_cast<quint64>(data[3]);
			headerSize += 2;
		} else if (payloadLen == 127) {
			if (m_readBuffer.size() < headerSize + 8)
				return;
			payloadLen = 0;
			for (int i = 0; i < 8; ++i) {
				payloadLen = (payloadLen << 8) | static_cast<quint64>(data[headerSize + i]);
			}
			headerSize += 8;
		}

		quint8 maskKey[4] = {0, 0, 0, 0};
		if (masked) {
			if (m_readBuffer.size() < headerSize + 4)
				return;
			for (int i = 0; i < 4; ++i) {
				maskKey[i] = data[headerSize + i];
			}
			headerSize += 4;
		}

		if (static_cast<quint64>(m_readBuffer.size()) < static_cast<quint64>(headerSize) + payloadLen) {
			return;
		}

		QByteArray payload = m_readBuffer.mid(headerSize, static_cast<qsizetype>(payloadLen));
		m_readBuffer.remove(0, headerSize + static_cast<qsizetype>(payloadLen));

		if (masked) {
			for (qsizetype i = 0; i < payload.size(); ++i) {
				payload[i] = static_cast<char>(payload[i] ^ maskKey[i % 4]);
			}
		}

		if (opcode == 0x1) {
			m_currentTextMessage = payload;
			if (fin) {
				emit textMessageReceived(QString::fromUtf8(m_currentTextMessage));
				m_currentTextMessage.clear();
			}
		} else if (opcode == 0x0) {
			m_currentTextMessage.append(payload);
			if (fin) {
				emit textMessageReceived(QString::fromUtf8(m_currentTextMessage));
				m_currentTextMessage.clear();
			}
		} else if (opcode == 0x9) {
			sendFrame(0xA, payload);
		} else if (opcode == 0x8) {
			sendFrame(0x8, payload);
			m_socket->disconnectFromHost();
		}
	}
}

void SimpleWebSocketClient::sendFrame(quint8 opcode, const QByteArray &payload)
{
	if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
		return;

	QByteArray frame;
	frame.append(static_cast<char>(0x80 | (opcode & 0x0F)));

	quint64 len = static_cast<quint64>(payload.size());
	if (len <= 125) {
		frame.append(static_cast<char>(0x80 | static_cast<quint8>(len)));
	} else if (len <= 0xFFFF) {
		frame.append(static_cast<char>(0x80 | 126));
		frame.append(static_cast<char>((len >> 8) & 0xFF));
		frame.append(static_cast<char>(len & 0xFF));
	} else {
		frame.append(static_cast<char>(0x80 | 127));
		for (int i = 7; i >= 0; --i) {
			frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
		}
	}

	quint32 mask = QRandomGenerator::global()->generate();
	quint8 maskBytes[4];
	maskBytes[0] = static_cast<quint8>((mask >> 24) & 0xFF);
	maskBytes[1] = static_cast<quint8>((mask >> 16) & 0xFF);
	maskBytes[2] = static_cast<quint8>((mask >> 8) & 0xFF);
	maskBytes[3] = static_cast<quint8>(mask & 0xFF);

	for (int i = 0; i < 4; ++i) {
		frame.append(static_cast<char>(maskBytes[i]));
	}

	QByteArray maskedPayload(payload.size(), 0);
	for (qsizetype i = 0; i < payload.size(); ++i) {
		maskedPayload[i] = static_cast<char>(payload[i] ^ maskBytes[i % 4]);
	}
	frame.append(maskedPayload);

	m_socket->write(frame);
}

qint64 SimpleWebSocketClient::sendTextMessage(const QString &message)
{
	QByteArray utf8 = message.toUtf8();
	sendFrame(0x1, utf8);
	return utf8.size();
}

} // namespace Nicolive

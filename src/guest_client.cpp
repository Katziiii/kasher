#include "guest_client.h"
#include "protocol.h"

#include <QJsonDocument>
#include <QUrl>

GuestClient::GuestClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(
          QString(),
          QWebSocketProtocol::VersionLatest,
          this))
{
    connect(m_socket, &QWebSocket::connected,
            this,     &GuestClient::onConnected);
    connect(m_socket, &QWebSocket::disconnected,
            this,     &GuestClient::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this,     &GuestClient::onMessage);
}

GuestClient::~GuestClient() {
    m_socket->close();
}

void GuestClient::connectToHost(const QString &host, quint16 port,
                                 const QString &localId,
                                 const QString &name,
                                 const QString &color) {
    m_localId = localId;
    m_name    = name;
    m_color   = color;
    m_socket->open(QUrl(QStringLiteral("ws://%1:%2").arg(host).arg(port)));
}

void GuestClient::disconnectFromHost() {
    m_socket->close();
}

bool GuestClient::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void GuestClient::onConnected() {
    QJsonObject msg;
    msg[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Join);
    msg[QStringLiteral("author")] = m_localId;
    msg[QStringLiteral("name")]   = m_name;
    msg[QStringLiteral("color")]  = m_color;
    sendJson(msg);
    Q_EMIT connected();
}

void GuestClient::onDisconnected() {
    Q_EMIT disconnected();
}

void GuestClient::onMessage(const QString &raw) {
    QJsonObject msg  = QJsonDocument::fromJson(raw.toUtf8()).object();
    QString     type = msg[QStringLiteral("type")].toString();

    if (type == QLatin1String(Protocol::Msg::Sync)) {
        Q_EMIT syncReceived(
            msg[QStringLiteral("content")].toString(),
            msg[QStringLiteral("rev")].toInt(),
            msg[QStringLiteral("participants")].toArray()
        );
    } else if (type == QLatin1String(Protocol::Msg::Op)) {
        Q_EMIT opReceived(msg);
    } else if (type == QLatin1String(Protocol::Msg::Cursor)) {
        Q_EMIT cursorReceived(msg);
    } else if (type == QLatin1String(Protocol::Msg::Joined)) {
        Q_EMIT participantJoined(msg);
    } else if (type == QLatin1String(Protocol::Msg::Left)) {
        Q_EMIT participantLeft(msg[QStringLiteral("author")].toString());
    } else if (type == QLatin1String(Protocol::Msg::FileOpen)) {
        Q_EMIT fileOpenReceived(
            msg[QStringLiteral("url")].toString(),
            msg[QStringLiteral("content")].toString(),
            msg[QStringLiteral("rev")].toInt()
        );
    }
    // Ack is intentionally handled in Session, not here
}

void GuestClient::sendOp(const QJsonObject &opMsg) {
    sendJson(opMsg);
}

void GuestClient::sendCursor(int line, int col, const QString &author) {
    QJsonObject msg;
    msg[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Cursor);
    msg[QStringLiteral("author")] = author;
    msg[QStringLiteral("line")]   = line;
    msg[QStringLiteral("col")]    = col;
    sendJson(msg);
}

void GuestClient::sendJson(const QJsonObject &msg) {
    m_socket->sendTextMessage(
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
}

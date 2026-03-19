#include "host_server.h"
#include "protocol.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>

HostServer::HostServer(QObject *parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(
          QStringLiteral("kate-liveshare"),
          QWebSocketServer::NonSecureMode,
          this))
{
    connect(m_server, &QWebSocketServer::newConnection,
            this,     &HostServer::onNewConnection);
}

HostServer::~HostServer() {
    stop();
}

bool HostServer::start(quint16 port) {
    if (m_server->listen(QHostAddress::Any, port))
        return true;
    return m_server->listen(QHostAddress::Any, 0);
}

void HostServer::stop() {
    for (auto *peer : m_peers.keys())
        peer->close();
    m_peers.clear();
    m_server->close();
}

bool HostServer::isRunning() const {
    return m_server->isListening();
}

quint16 HostServer::port() const {
    return m_server->serverPort();
}

void HostServer::onNewConnection() {
    QWebSocket *peer = m_server->nextPendingConnection();
    m_peers[peer]    = {};

    connect(peer, &QWebSocket::textMessageReceived,
            this, &HostServer::onMessage);
    connect(peer, &QWebSocket::disconnected,
            this, &HostServer::onPeerDisconnected);
}

void HostServer::onMessage(const QString &raw) {
    auto *peer = qobject_cast<QWebSocket *>(sender());
    if (!peer) return;

    QJsonObject msg  = QJsonDocument::fromJson(raw.toUtf8()).object();
    QString     type = msg[QStringLiteral("type")].toString();

    if (type == QLatin1String(Protocol::Msg::Join)) {
        PeerInfo info;
        info.id    = msg[QStringLiteral("author")].toString();
        info.name  = msg[QStringLiteral("name")].toString();
        info.color = msg[QStringLiteral("color")].toString();
        m_peers[peer] = info;
        Q_EMIT peerJoined(peer, info.id, info.name, info.color);

    } else if (type == QLatin1String(Protocol::Msg::Op)) {
        Q_EMIT opReceived(msg, peer);

    } else if (type == QLatin1String(Protocol::Msg::Cursor)) {
        Q_EMIT cursorReceived(msg, peer);
        // forward cursor to everyone else
        broadcastJson(msg, peer);
    }
}

void HostServer::onPeerDisconnected() {
    auto *peer = qobject_cast<QWebSocket *>(sender());
    if (!peer) return;

    QString id = m_peers.value(peer).id;
    m_peers.remove(peer);
    peer->deleteLater();

    if (!id.isEmpty()) {
        QJsonObject msg;
        msg[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Left);
        msg[QStringLiteral("author")] = id;
        broadcastJson(msg);
        Q_EMIT peerLeft(peer, id);
    }
}

void HostServer::broadcastJson(const QJsonObject &msg, QWebSocket *exclude) {
    QString data = serialize(msg);
    for (auto *peer : m_peers.keys()) {
        if (peer != exclude)
            peer->sendTextMessage(data);
    }
}

void HostServer::sendJson(QWebSocket *peer, const QJsonObject &msg) {
    peer->sendTextMessage(serialize(msg));
}

void HostServer::sendSync(QWebSocket *peer, const QString &docContent,
                           int rev, const QList<QJsonObject> &participants) {
    QJsonArray arr;
    for (const auto &p : participants) arr.append(p);

    QJsonObject msg;
    msg[QStringLiteral("type")]         = QLatin1String(Protocol::Msg::Sync);
    msg[QStringLiteral("content")]      = docContent;
    msg[QStringLiteral("rev")]          = rev;
    msg[QStringLiteral("participants")] = arr;
    sendJson(peer, msg);
}

QString HostServer::serialize(const QJsonObject &obj) {
    return QString::fromUtf8(
        QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

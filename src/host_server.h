#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QWebSocket>
#include <QWebSocketServer>

class HostServer : public QObject {
    Q_OBJECT
public:
    explicit HostServer(QObject *parent = nullptr);
    ~HostServer() override;

    bool    start(quint16 port = 6789);
    void    stop();
    bool    isRunning() const;
    quint16 port() const;

    void broadcastJson(const QJsonObject &msg, QWebSocket *exclude = nullptr);
    void sendJson(QWebSocket *peer, const QJsonObject &msg);
    void sendSync(QWebSocket *peer, const QString &docContent, int rev,
                  const QList<QJsonObject> &participants);

Q_SIGNALS:
    void opReceived(QJsonObject msg, QWebSocket *sender);
    void cursorReceived(QJsonObject msg, QWebSocket *sender);
    void peerJoined(QWebSocket *peer, QString id, QString name, QString color);
    void peerLeft(QWebSocket *peer, QString id);

private Q_SLOTS:
    void onNewConnection();
    void onMessage(const QString &raw);
    void onPeerDisconnected();

private:
    struct PeerInfo { QString id, name, color; };

    static QString serialize(const QJsonObject &obj);

    QWebSocketServer           *m_server;
    QMap<QWebSocket*, PeerInfo> m_peers;
};

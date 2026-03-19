#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QWebSocket>

class GuestClient : public QObject {
    Q_OBJECT
public:
    explicit GuestClient(QObject *parent = nullptr);
    ~GuestClient() override;

    void connectToHost(const QString &host, quint16 port,
                       const QString &localId,
                       const QString &name,
                       const QString &color);
    void disconnectFromHost();
    bool isConnected() const;

    void sendOp(const QJsonObject &opMsg);
    void sendCursor(int line, int col, const QString &author);

Q_SIGNALS:
    void connected();
    void disconnected();
    void syncReceived(QString content, int rev, QJsonArray participants);
    void opReceived(QJsonObject msg);
    void cursorReceived(QJsonObject msg);
    void participantJoined(QJsonObject msg);
    void participantLeft(QString id);
    void fileOpenReceived(QString url, QString content, int rev);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onMessage(const QString &raw);

private:
    void sendJson(const QJsonObject &msg);

    QWebSocket *m_socket;
    QString     m_localId;
    QString     m_name;
    QString     m_color;
};

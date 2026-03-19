#pragma once

#include "ot_engine.h"
#include "participant.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>

#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <KTextEditor/View>

class HostServer;
class GuestClient;
class QWebSocket;

class Session : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Hosting, Joined };

    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    bool startHosting(quint16 port = 6789);
    bool joinSession(const QString &host, quint16 port, const QString &name);
    void stopSession();

    State   state()      const { return m_state; }
    quint16 hostPort()   const;
    QString localId()    const { return m_localId; }
    QString localName()  const { return m_localName; }
    QString localColor() const { return m_localColor; }

    void attachDocument(KTextEditor::Document *doc, KTextEditor::View *view);
    void detachDocument();
    void notifyFileOpen(KTextEditor::Document *doc);
    void applyFileContent(const QString &content); // guest: overwrite doc with host content

    QList<Participant *> participants() const { return m_participants.values(); }

Q_SIGNALS:
    void stateChanged(State newState);
    void participantAdded(Participant *p);
    void participantRemoved(QString id);
    void logMessage(QString msg);
    void fileOpenRequested(QString url, QString content);

private Q_SLOTS:
    void onDocumentChanged(KTextEditor::Document *doc);
    void onViewCursorMoved(KTextEditor::View *view, const KTextEditor::Cursor &cursor);

    // Host slots
    void onHostOpReceived(QJsonObject msg, QWebSocket *sender);
    void onHostCursorReceived(QJsonObject msg, QWebSocket *sender);
    void onPeerJoined(QWebSocket *peer, QString id, QString name, QString color);
    void onPeerLeft(QWebSocket *peer, QString id);

    // Guest slots
    void onGuestSyncReceived(QString content, int rev, QJsonArray participants);
    void onGuestOpReceived(QJsonObject msg);
    void onGuestCursorReceived(QJsonObject msg);
    void onGuestParticipantJoined(QJsonObject msg);
    void onGuestParticipantLeft(QString id);
    void onGuestFileOpenReceived(QString url, QString content, int rev);

private:
    Op          opFromJson(const QJsonObject &msg) const;
    QJsonObject opToJson(const Op &op) const;
    void        sendOp(const Op &op);
    void        applySync(const QString &content, int rev, const QJsonArray &participants);
    void        applyRemoteOp(const Op &op);
    void        handleRemoteCursor(const QString &authorId, int line, int col);
    void        addParticipant(const QString &id, const QString &name, const QString &color);
    void        removeParticipant(const QString &id);

    KTextEditor::Cursor posToKCursor(int pos) const;

    State  m_state     = State::Idle;
    bool    m_applying  = false;
    int     m_guestRev  = 0;

    // pending sync received before a document was attached
    bool    m_hasPendingSync = false;
    QString m_pendingSyncContent;
    int     m_pendingSyncRev = 0;
    QJsonArray m_pendingSyncParticipants;

    QString m_localId;
    QString m_localName;
    QString m_localColor;

    KTextEditor::Document *m_doc  = nullptr;
    KTextEditor::View     *m_view = nullptr;
    QString                m_shadow;   // mirror of doc->text() for diffing

    OTEngine    m_ot;   // used only in host mode for global history
    HostServer *m_host  = nullptr;
    GuestClient *m_guest = nullptr;

    QMap<QString, Participant *> m_participants;
    QMap<QWebSocket *, QString>  m_peerToId;
};

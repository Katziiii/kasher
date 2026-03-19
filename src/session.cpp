#include "session.h"
#include "guest_client.h"
#include "host_server.h"
#include "protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

// ── Construction / destruction ───────────────────────────────────────────────

Session::Session(QObject *parent)
    : QObject(parent)
    , m_localId(Protocol::genUuid())
    , m_localName(QStringLiteral("Me"))
    , m_localColor(Protocol::genColor())
{}

Session::~Session() {
    stopSession();
    detachDocument();
}

// ── Session lifecycle ────────────────────────────────────────────────────────

bool Session::startHosting(quint16 port) {
    if (m_state != State::Idle) return false;

    m_host = new HostServer(this);
    connect(m_host, &HostServer::opReceived,     this, &Session::onHostOpReceived);
    connect(m_host, &HostServer::cursorReceived, this, &Session::onHostCursorReceived);
    connect(m_host, &HostServer::peerJoined,     this, &Session::onPeerJoined);
    connect(m_host, &HostServer::peerLeft,       this, &Session::onPeerLeft);

    if (!m_host->start(port)) {
        delete m_host;
        m_host = nullptr;
        return false;
    }

    m_ot.reset();
    m_state = State::Hosting;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("Hosting on port %1").arg(m_host->port()));
    return true;
}

bool Session::joinSession(const QString &host, quint16 port, const QString &name) {
    if (m_state != State::Idle) return false;

    m_localName = name.trimmed().isEmpty() ? QStringLiteral("Guest") : name.trimmed();

    m_guest = new GuestClient(this);
    connect(m_guest, &GuestClient::syncReceived,       this, &Session::onGuestSyncReceived);
    connect(m_guest, &GuestClient::opReceived,         this, &Session::onGuestOpReceived);
    connect(m_guest, &GuestClient::cursorReceived,     this, &Session::onGuestCursorReceived);
    connect(m_guest, &GuestClient::participantJoined,  this, &Session::onGuestParticipantJoined);
    connect(m_guest, &GuestClient::participantLeft,    this, &Session::onGuestParticipantLeft);
    connect(m_guest, &GuestClient::fileOpenReceived,   this, &Session::onGuestFileOpenReceived);
    connect(m_guest, &GuestClient::disconnected, this, [this]() {
        Q_EMIT logMessage(QStringLiteral("Disconnected from host."));
        stopSession();
    });

    m_guest->connectToHost(host, port, m_localId, m_localName, m_localColor);
    m_state = State::Joined;
    Q_EMIT stateChanged(m_state);
    return true;
}

void Session::stopSession() {
    if (m_state == State::Idle) return;

    for (auto *p : m_participants) {
        p->detach();
        delete p;
    }
    m_participants.clear();
    m_peerToId.clear();

    if (m_host) { m_host->stop(); delete m_host; m_host = nullptr; }
    if (m_guest) { m_guest->disconnectFromHost(); delete m_guest; m_guest = nullptr; }

    m_ot.reset();
    m_guestRev = 0;
    m_state    = State::Idle;
    Q_EMIT stateChanged(m_state);
    Q_EMIT logMessage(QStringLiteral("Session ended."));
}

quint16 Session::hostPort() const {
    return m_host ? m_host->port() : 0;
}

// ── Document attachment ──────────────────────────────────────────────────────

void Session::attachDocument(KTextEditor::Document *doc, KTextEditor::View *view) {
    detachDocument();
    m_doc    = doc;
    m_view   = view;
    m_shadow = doc ? doc->text() : QString();

    if (doc) {
        connect(doc, &KTextEditor::Document::textChanged,
                this, &Session::onDocumentChanged);

        // apply any sync that arrived before a document was open
        if (m_hasPendingSync) {
            m_hasPendingSync = false;
            applySync(m_pendingSyncContent, m_pendingSyncRev, m_pendingSyncParticipants);
            m_pendingSyncContent.clear();
            m_pendingSyncParticipants = QJsonArray();
        }
    }
    if (view) {
        connect(view, &KTextEditor::View::cursorPositionChanged,
                this, &Session::onViewCursorMoved);
    }
}

void Session::applySync(const QString &content, int rev, const QJsonArray &participants) {
    if (!m_doc) return;

    m_applying = true;
    m_doc->setText(content);
    m_applying = false;

    m_shadow   = content;
    m_guestRev = rev;

    for (const auto &val : participants) {
        QJsonObject p = val.toObject();
        addParticipant(
            p[QStringLiteral("id")].toString(),
            p[QStringLiteral("name")].toString(),
            p[QStringLiteral("color")].toString()
        );
    }
    Q_EMIT logMessage(QStringLiteral("Synced with host (rev %1).").arg(rev));
}

void Session::applyFileContent(const QString &content) {
    if (!m_doc) return;
    m_applying = true;
    m_doc->setText(content);
    m_applying = false;
    m_shadow = content;
}

void Session::notifyFileOpen(KTextEditor::Document *doc) {
    if (m_state != State::Hosting || !doc) return;

    QJsonObject msg;
    msg[QStringLiteral("type")]    = QLatin1String(Protocol::Msg::FileOpen);
    msg[QStringLiteral("url")]     = doc->url().toString();
    msg[QStringLiteral("content")] = doc->text();
    msg[QStringLiteral("rev")]     = m_ot.revision();
    m_host->broadcastJson(msg);
}

void Session::detachDocument() {
    if (m_doc) {
        disconnect(m_doc, nullptr, this, nullptr);
        for (auto *p : m_participants) p->detach();
    }
    if (m_view) {
        disconnect(m_view, nullptr, this, nullptr);
    }
    m_doc    = nullptr;
    m_view   = nullptr;
    m_shadow.clear();
}

// ── Local document change → op ───────────────────────────────────────────────

void Session::onDocumentChanged(KTextEditor::Document *doc) {
    if (m_applying || m_state == State::Idle) return;
    if (!doc) return;

    QString newText = doc->text();
    if (newText == m_shadow) return;

    const int oldLen = m_shadow.length();
    const int newLen = newText.length();
    const int minLen = qMin(oldLen, newLen);

    // common prefix
    int i = 0;
    while (i < minLen && m_shadow[i] == newText[i]) i++;

    // common suffix (don't exceed what's left)
    int j = 0;
    while (j < (minLen - i) &&
           m_shadow[oldLen - 1 - j] == newText[newLen - 1 - j]) j++;

    int     delLen  = oldLen - i - j;
    QString insText = newText.mid(i, newLen - i - j);

    int curRev = (m_state == State::Hosting) ? m_ot.revision() : m_guestRev;

    if (delLen > 0) {
        Op op;
        op.type     = Op::Delete;
        op.pos      = i;
        op.len      = delLen;
        op.revision = curRev;
        op.author   = m_localId;
        sendOp(op);
        if (m_state == State::Hosting) m_ot.commitOp(op);
    }
    if (!insText.isEmpty()) {
        Op op;
        op.type     = Op::Insert;
        op.pos      = i;
        op.text     = insText;
        op.revision = curRev;
        op.author   = m_localId;
        sendOp(op);
        if (m_state == State::Hosting) m_ot.commitOp(op);
    }

    m_shadow = newText;
}

void Session::onViewCursorMoved(KTextEditor::View *view,
                                 const KTextEditor::Cursor &cursor) {
    Q_UNUSED(view)
    if (m_state == State::Idle) return;

    QJsonObject msg;
    msg[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Cursor);
    msg[QStringLiteral("author")] = m_localId;
    msg[QStringLiteral("line")]   = cursor.line();
    msg[QStringLiteral("col")]    = cursor.column();

    if (m_host)
        m_host->broadcastJson(msg);
    else if (m_guest)
        m_guest->sendCursor(cursor.line(), cursor.column(), m_localId);
}

// ── Host event handlers ──────────────────────────────────────────────────────

void Session::onHostOpReceived(QJsonObject msg, QWebSocket *sender) {
    Op op = opFromJson(msg);
    if (op.isNoop()) return;

    Op transformed = m_ot.transformAgainstHistory(op, op.revision);
    if (transformed.isNoop()) return;

    m_ot.commitOp(transformed);
    applyRemoteOp(transformed);

    // broadcast the transformed op with the new global revision
    QJsonObject broadcast = opToJson(transformed);
    broadcast[QStringLiteral("rev")] = m_ot.revision();
    m_host->broadcastJson(broadcast, sender);

    // ack sender with new revision so it can track correctly
    QJsonObject ack;
    ack[QStringLiteral("type")] = QLatin1String(Protocol::Msg::Ack);
    ack[QStringLiteral("rev")]  = m_ot.revision();
    m_host->sendJson(sender, ack);
}

void Session::onHostCursorReceived(QJsonObject msg, QWebSocket *) {
    handleRemoteCursor(
        msg[QStringLiteral("author")].toString(),
        msg[QStringLiteral("line")].toInt(),
        msg[QStringLiteral("col")].toInt()
    );
}

void Session::onPeerJoined(QWebSocket *peer,
                            QString id, QString name, QString color) {
    m_peerToId[peer] = id;
    addParticipant(id, name, color);

    // build current participant list for the sync message
    QList<QJsonObject> others;
    for (auto *p : m_participants) {
        if (p->id == id) continue;
        QJsonObject info;
        info[QStringLiteral("id")]    = p->id;
        info[QStringLiteral("name")]  = p->name;
        info[QStringLiteral("color")] = p->color.name();
        others.append(info);
    }
    QJsonObject hostInfo;
    hostInfo[QStringLiteral("id")]    = m_localId;
    hostInfo[QStringLiteral("name")]  = m_localName;
    hostInfo[QStringLiteral("color")] = m_localColor;
    others.append(hostInfo);

    m_host->sendSync(peer, m_doc ? m_doc->text() : QString(),
                     m_ot.revision(), others);

    // tell everyone else someone joined
    QJsonObject joined;
    joined[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Joined);
    joined[QStringLiteral("author")] = id;
    joined[QStringLiteral("name")]   = name;
    joined[QStringLiteral("color")]  = color;
    m_host->broadcastJson(joined, peer);

    Q_EMIT logMessage(QStringLiteral("%1 joined.").arg(name));
}

void Session::onPeerLeft(QWebSocket *peer, QString id) {
    m_peerToId.remove(peer);
    removeParticipant(id);
    Q_EMIT logMessage(QStringLiteral("A participant left."));
}

// ── Guest event handlers ─────────────────────────────────────────────────────

void Session::onGuestSyncReceived(QString content, int rev, QJsonArray participants) {
    if (!m_doc) {
        m_hasPendingSync          = true;
        m_pendingSyncContent      = content;
        m_pendingSyncRev          = rev;
        m_pendingSyncParticipants = participants;
        return;
    }
    applySync(content, rev, participants);
}

void Session::onGuestOpReceived(QJsonObject msg) {
    QString author = msg[QStringLiteral("author")].toString();

    // Update our known revision from the global rev tag the host stamps on ops
    int newRev = msg[QStringLiteral("rev")].toInt(m_guestRev);
    m_guestRev = newRev;

    // Skip ops we originated ourselves — we already applied them locally
    if (author == m_localId) return;

    Op op = opFromJson(msg);
    if (op.isNoop()) return;
    applyRemoteOp(op);
}

void Session::onGuestCursorReceived(QJsonObject msg) {
    handleRemoteCursor(
        msg[QStringLiteral("author")].toString(),
        msg[QStringLiteral("line")].toInt(),
        msg[QStringLiteral("col")].toInt()
    );
}

void Session::onGuestParticipantJoined(QJsonObject msg) {
    addParticipant(
        msg[QStringLiteral("author")].toString(),
        msg[QStringLiteral("name")].toString(),
        msg[QStringLiteral("color")].toString()
    );
}

void Session::onGuestParticipantLeft(QString id) {
    removeParticipant(id);
}

void Session::onGuestFileOpenReceived(QString url, QString content, int rev) {
    m_guestRev = rev;
    m_ot.setBaseRevision(rev);
    // Signal the plugin view to open the file — it has the MainWindow reference
    Q_EMIT fileOpenRequested(url, content);
}

// ── Private helpers ──────────────────────────────────────────────────────────

void Session::sendOp(const Op &op) {
    QJsonObject msg = opToJson(op);
    if (m_host)
        m_host->broadcastJson(msg);
    else if (m_guest)
        m_guest->sendOp(msg);
}

void Session::applyRemoteOp(const Op &op) {
    if (!m_doc || op.isNoop()) return;

    m_applying = true;

    if (op.type == Op::Insert) {
        KTextEditor::Cursor cur = posToKCursor(op.pos);
        m_doc->insertText(cur, op.text);
        m_shadow.insert(op.pos, op.text);

    } else if (op.type == Op::Delete) {
        int safeLen = qMin(op.len, (int)m_shadow.length() - op.pos);
        if (safeLen > 0) {
            KTextEditor::Cursor start = posToKCursor(op.pos);
            KTextEditor::Cursor end   = posToKCursor(op.pos + safeLen);
            m_doc->removeText(KTextEditor::Range(start, end));
            m_shadow.remove(op.pos, safeLen);
        }
    }

    m_applying = false;
}

void Session::handleRemoteCursor(const QString &authorId, int line, int col) {
    if (authorId == m_localId || !m_doc) return;
    auto *p = m_participants.value(authorId);
    if (p) p->updateCursor(m_doc, line, col);
}

void Session::addParticipant(const QString &id, const QString &name, const QString &color) {
    if (id == m_localId || m_participants.contains(id)) return;
    auto *p  = new Participant();
    p->id    = id;
    p->name  = name;
    p->color = QColor(color);
    m_participants[id] = p;
    Q_EMIT participantAdded(p);
}

void Session::removeParticipant(const QString &id) {
    auto *p = m_participants.take(id);
    if (!p) return;
    if (m_doc) p->detach();
    delete p;
    Q_EMIT participantRemoved(id);
}

KTextEditor::Cursor Session::posToKCursor(int pos) const {
    int line = 0, col = 0;
    for (int i = 0; i < pos && i < (int)m_shadow.length(); i++) {
        if (m_shadow[i] == QLatin1Char('\n')) { line++; col = 0; }
        else ++col;
    }
    return KTextEditor::Cursor(line, col);
}

Op Session::opFromJson(const QJsonObject &msg) const {
    Op op;
    op.revision = msg[QStringLiteral("rev")].toInt();
    op.author   = msg[QStringLiteral("author")].toString();
    op.pos      = msg[QStringLiteral("pos")].toInt();

    QString opType = msg[QStringLiteral("op_type")].toString();
    if (opType == QLatin1String(Protocol::OpType::Insert)) {
        op.type = Op::Insert;
        op.text = msg[QStringLiteral("text")].toString();
    } else if (opType == QLatin1String(Protocol::OpType::Delete)) {
        op.type = Op::Delete;
        op.len  = msg[QStringLiteral("len")].toInt();
    }
    return op;
}

QJsonObject Session::opToJson(const Op &op) const {
    QJsonObject msg;
    msg[QStringLiteral("type")]   = QLatin1String(Protocol::Msg::Op);
    msg[QStringLiteral("author")] = op.author;
    msg[QStringLiteral("pos")]    = op.pos;
    msg[QStringLiteral("rev")]    = op.revision;

    if (op.type == Op::Insert) {
        msg[QStringLiteral("op_type")] = QLatin1String(Protocol::OpType::Insert);
        msg[QStringLiteral("text")]    = op.text;
    } else {
        msg[QStringLiteral("op_type")] = QLatin1String(Protocol::OpType::Delete);
        msg[QStringLiteral("len")]     = op.len;
    }
    return msg;
}

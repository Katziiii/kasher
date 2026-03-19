#pragma once

#include "session.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

class ToolView : public QWidget {
    Q_OBJECT
public:
    explicit ToolView(Session *session, QWidget *parent = nullptr);

private Q_SLOTS:
    void onStateChanged(Session::State state);
    void onParticipantAdded(Participant *p);
    void onParticipantRemoved(const QString &id);
    void onLog(const QString &msg);

    void onHostClicked();
    void onJoinClicked();
    void onStopClicked();
    void onCopyLocalClicked();
    void onShareInternetClicked();
    void onBoreOutput();
    void onBoreFinished();

private:
    void stopTunnel();

    Session  *m_session;
    QProcess *m_bore = nullptr;

    QWidget        *m_idlePage   = nullptr;
    QWidget        *m_activePage = nullptr;
    QStackedWidget *m_stack      = nullptr;

    // Idle page
    QLineEdit   *m_nameEdit  = nullptr;
    QLineEdit   *m_portEdit  = nullptr;
    QLineEdit   *m_hostEdit  = nullptr;
    QPushButton *m_hostBtn   = nullptr;
    QPushButton *m_joinBtn   = nullptr;

    // Active page
    QLabel      *m_statusLabel   = nullptr;
    QPushButton *m_copyLocalBtn  = nullptr;
    QPushButton *m_shareInetBtn  = nullptr;
    QPushButton *m_stopBtn       = nullptr;
    QListWidget *m_partList      = nullptr;
    QLabel      *m_logLabel      = nullptr;
};

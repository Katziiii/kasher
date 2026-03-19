#include "tool_view.h"

#include <QApplication>
#include <QClipboard>
#include <QGroupBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QNetworkInterface>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QVBoxLayout>

static QPixmap colorDot(const QColor &c, int size = 10) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);
    return px;
}

static QString localIp() {
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
        for (const auto &addr : iface.addressEntries()) {
            if (addr.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return addr.ip().toString();
        }
    }
    return QStringLiteral("127.0.0.1");
}

static QWidget *labeled(const QString &text, QWidget *w) {
    auto *wrap   = new QWidget;
    auto *layout = new QVBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet(QStringLiteral("font-size: 10px; color: gray;"));
    layout->addWidget(lbl);
    layout->addWidget(w);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return wrap;
}

ToolView::ToolView(Session *session, QWidget *parent)
: QWidget(parent)
, m_session(session)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ── Idle page ─────────────────────────────────────────────────────────────
    m_idlePage       = new QWidget;
    auto *idleLayout = new QVBoxLayout(m_idlePage);
    idleLayout->setContentsMargins(6, 6, 6, 6);
    idleLayout->setSpacing(8);

    m_nameEdit = new QLineEdit(session->localName());
    m_nameEdit->setPlaceholderText(tr("Your display name"));
    idleLayout->addWidget(labeled(tr("YOUR NAME"), m_nameEdit));

    auto *hostGroup  = new QGroupBox(tr("Host"));
    auto *hostLayout = new QVBoxLayout(hostGroup);
    hostLayout->setSpacing(4);
    m_portEdit = new QLineEdit(QStringLiteral("6789"));
    m_portEdit->setPlaceholderText(tr("Port"));
    hostLayout->addWidget(labeled(tr("PORT"), m_portEdit));
    m_hostBtn = new QPushButton(tr("Start Hosting"));
    m_hostBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hostLayout->addWidget(m_hostBtn);
    idleLayout->addWidget(hostGroup);

    auto *joinGroup  = new QGroupBox(tr("Join"));
    auto *joinLayout = new QVBoxLayout(joinGroup);
    joinLayout->setSpacing(4);
    m_hostEdit = new QLineEdit;
    m_hostEdit->setPlaceholderText(tr("ip:port  or  bore.pub:XXXXX"));
    joinLayout->addWidget(labeled(tr("ADDRESS"), m_hostEdit));
    m_joinBtn = new QPushButton(tr("Join Session"));
    m_joinBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    joinLayout->addWidget(m_joinBtn);
    idleLayout->addWidget(joinGroup);

    idleLayout->addStretch();

    // ── Active page ───────────────────────────────────────────────────────────
    m_activePage       = new QWidget;
    auto *activeLayout = new QVBoxLayout(m_activePage);
    activeLayout->setContentsMargins(6, 6, 6, 6);
    activeLayout->setSpacing(6);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        QStringLiteral("padding: 4px; border-radius: 4px; background: palette(mid);"));
    activeLayout->addWidget(m_statusLabel);

    // ── Share buttons ─────────────────────────────────────────────────────────
    m_copyLocalBtn = new QPushButton(tr("⎘  Copy Local Invite"));
    m_copyLocalBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_copyLocalBtn->setToolTip(tr("Copy LAN address (same network only)"));
    activeLayout->addWidget(m_copyLocalBtn);

    m_shareInetBtn = new QPushButton(tr("🌐  Share via Internet"));
    m_shareInetBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_shareInetBtn->setToolTip(tr("Create a public tunnel via bore.pub — works across the internet"));
    activeLayout->addWidget(m_shareInetBtn);

    m_stopBtn = new QPushButton(tr("✕  Stop Session"));
    m_stopBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_stopBtn->setStyleSheet(
        QStringLiteral("QPushButton { color: #e74c3c; font-weight: bold; }"));
    activeLayout->addWidget(m_stopBtn);

    auto *partGroup  = new QGroupBox(tr("Participants"));
    auto *partLayout = new QVBoxLayout(partGroup);
    partLayout->setContentsMargins(4, 4, 4, 4);
    m_partList = new QListWidget;
    m_partList->setSelectionMode(QAbstractItemView::NoSelection);
    m_partList->setFrameShape(QFrame::NoFrame);
    m_partList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    partLayout->addWidget(m_partList);
    activeLayout->addWidget(partGroup, 1);

    m_logLabel = new QLabel;
    m_logLabel->setWordWrap(true);
    m_logLabel->setAlignment(Qt::AlignCenter);
    m_logLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    activeLayout->addWidget(m_logLabel);

    // ── Stack ─────────────────────────────────────────────────────────────────
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_idlePage);
    m_stack->addWidget(m_activePage);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_hostBtn,      &QPushButton::clicked, this, &ToolView::onHostClicked);
    connect(m_joinBtn,      &QPushButton::clicked, this, &ToolView::onJoinClicked);
    connect(m_stopBtn,      &QPushButton::clicked, this, &ToolView::onStopClicked);
    connect(m_copyLocalBtn, &QPushButton::clicked, this, &ToolView::onCopyLocalClicked);
    connect(m_shareInetBtn, &QPushButton::clicked, this, &ToolView::onShareInternetClicked);

    connect(session, &Session::stateChanged,       this, &ToolView::onStateChanged);
    connect(session, &Session::participantAdded,   this, &ToolView::onParticipantAdded);
    connect(session, &Session::participantRemoved, this, &ToolView::onParticipantRemoved);
    connect(session, &Session::logMessage,         this, &ToolView::onLog);

    onStateChanged(session->state());
}

void ToolView::onStateChanged(Session::State state) {
    m_partList->clear();
    stopTunnel();

    if (state == Session::State::Idle) {
        m_stack->setCurrentIndex(0);
        return;
    }

    m_stack->setCurrentIndex(1);

    if (state == Session::State::Hosting) {
        m_statusLabel->setText(
            QStringLiteral("<b>Hosting</b><br><small>%1:%2</small>")
            .arg(localIp()).arg(m_session->hostPort()));
        m_copyLocalBtn->setVisible(true);
        m_shareInetBtn->setVisible(true);
    } else {
        m_statusLabel->setText(tr("<b>Connected</b><br><small>as guest</small>"));
        m_copyLocalBtn->setVisible(false);
        m_shareInetBtn->setVisible(false);
    }

    auto *selfItem = new QListWidgetItem(
        QIcon(colorDot(QColor(m_session->localColor()))),
                                         QStringLiteral("%1 (you)").arg(m_session->localName()));
    m_partList->addItem(selfItem);
}

void ToolView::onParticipantAdded(Participant *p) {
    auto *item = new QListWidgetItem(QIcon(colorDot(p->color)), p->name);
    item->setData(Qt::UserRole, p->id);
    m_partList->addItem(item);
}

void ToolView::onParticipantRemoved(const QString &id) {
    for (int i = 0; i < m_partList->count(); i++) {
        if (m_partList->item(i)->data(Qt::UserRole).toString() == id) {
            delete m_partList->takeItem(i);
            break;
        }
    }
}

void ToolView::onLog(const QString &msg) {
    m_logLabel->setText(msg);
}

void ToolView::onHostClicked() {
    bool ok   = false;
    quint16 p = (quint16)m_portEdit->text().toUInt(&ok);
    if (!ok || p == 0) p = 6789;
    if (!m_session->startHosting(p))
        m_logLabel->setText(tr("Failed to start server."));
}

void ToolView::onJoinClicked() {
    QString addr = m_hostEdit->text().trimmed();
    if (addr.isEmpty()) return;

    QString host;
    quint16 port = 6789;
    int colon    = addr.lastIndexOf(QLatin1Char(':'));
    if (colon != -1) {
        host = addr.left(colon);
        port = (quint16)addr.mid(colon + 1).toUInt();
    } else {
        host = addr;
    }
    m_session->joinSession(host, port, m_nameEdit->text().trimmed());
}

void ToolView::onStopClicked() {
    stopTunnel();
    m_session->stopSession();
}

void ToolView::onCopyLocalClicked() {
    QString invite = QStringLiteral("%1:%2").arg(localIp()).arg(m_session->hostPort());
    QApplication::clipboard()->setText(invite);
    m_logLabel->setText(tr("Local address copied — works on same network."));
}

void ToolView::onShareInternetClicked() {
    if (m_bore && m_bore->state() != QProcess::NotRunning) {
        // Already running — just copy the address again
        m_logLabel->setText(tr("Tunnel already active — check address above."));
        return;
    }

    // QStandardPaths uses Kate's stripped desktop PATH — search common locations too
    QString borePath = QStandardPaths::findExecutable(QStringLiteral("bore"));
    if (borePath.isEmpty()) {
        const QStringList extraPaths = {
            QStringLiteral("/usr/local/bin"),
            QStringLiteral("/usr/bin"),
            QDir::homePath() + QStringLiteral("/.cargo/bin"),
            QDir::homePath() + QStringLiteral("/.local/bin"),
            QStringLiteral("/opt/homebrew/bin"),   // macOS Homebrew (Apple Silicon)
            QStringLiteral("/usr/local/homebrew/bin"), // macOS Homebrew (Intel)
        };
        borePath = QStandardPaths::findExecutable(QStringLiteral("bore"), extraPaths);
    }
    if (borePath.isEmpty()) {
        m_logLabel->setText(
            tr("bore not found. Install with:\n"
            "cargo install bore-cli\n"
            "or download from github.com/ekzhang/bore/releases"));
        return;
    }

    m_shareInetBtn->setEnabled(false);
    m_logLabel->setText(tr("Opening tunnel via bore.pub…"));

    m_bore = new QProcess(this);
    connect(m_bore, &QProcess::readyReadStandardOutput, this, &ToolView::onBoreOutput);
    connect(m_bore, &QProcess::readyReadStandardError,  this, &ToolView::onBoreOutput);
    connect(m_bore, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,   &ToolView::onBoreFinished);

    // bore local <port> --to bore.pub
    m_bore->start(borePath, {
        QStringLiteral("local"),
                  QString::number(m_session->hostPort()),
                  QStringLiteral("--to"),
                  QStringLiteral("bore.pub")
    });
}

void ToolView::onBoreOutput() {
    if (!m_bore) return;

    QString output = QString::fromUtf8(m_bore->readAllStandardOutput())
    + QString::fromUtf8(m_bore->readAllStandardError());

    // bore prints: "listening at bore.pub:XXXXX"
    static QRegularExpression re(QStringLiteral("bore\\.pub:(\\d+)"));
    auto match = re.match(output);
    if (match.hasMatch()) {
        QString address = QStringLiteral("bore.pub:") + match.captured(1);
        QApplication::clipboard()->setText(address);
        m_statusLabel->setText(
            QStringLiteral("<b>Hosting</b><br><small>%1</small><br>"
            "<small style='color:#2ecc71'>🌐 %2</small>")
            .arg(QStringLiteral("%1:%2").arg(localIp()).arg(m_session->hostPort()))
            .arg(address));
        m_logLabel->setText(tr("Tunnel active! Address copied — share it with your friend."));
        m_shareInetBtn->setText(tr("🌐 Tunnel Active"));
    }
}

void ToolView::onBoreFinished() {
    m_logLabel->setText(tr("Tunnel closed."));
    m_shareInetBtn->setEnabled(true);
    m_shareInetBtn->setText(tr("🌐  Share via Internet"));
    m_bore->deleteLater();
    m_bore = nullptr;
}

void ToolView::stopTunnel() {
    if (m_bore && m_bore->state() != QProcess::NotRunning) {
        m_bore->terminate();
        m_bore->waitForFinished(1000);
        m_bore->deleteLater();
        m_bore = nullptr;
    }
}

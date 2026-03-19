#include "plugin.h"
#include "tool_view.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <QUrl>
#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(LiveSharePluginFactory,
                            "kateplugin_liveshare.json",
                            registerPlugin<LiveSharePlugin>();)

LiveSharePlugin::LiveSharePlugin(QObject *parent, const QVariantList &args)
    : KTextEditor::Plugin(parent)
{
    Q_UNUSED(args)
}

QObject *LiveSharePlugin::createView(KTextEditor::MainWindow *mainWindow) {
    return new LiveSharePluginView(this, mainWindow);
}

// ── LiveSharePluginView ──────────────────────────────────────────────────────

LiveSharePluginView::LiveSharePluginView(LiveSharePlugin *plugin,
                                          KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
    , m_session(new Session(this))
{
    Q_UNUSED(plugin)

    auto *tv = new ToolView(m_session);
    m_toolView = mainWindow->createToolView(
        plugin,
        QStringLiteral("kate_plugin_liveshare"),
        KTextEditor::MainWindow::Left,
        QIcon::fromTheme(QStringLiteral("document-share")),
        i18n("Live Share")
    );
    tv->setParent(m_toolView);

    auto *layout = new QVBoxLayout(m_toolView);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tv);

    // Host: when active file changes, tell guests to open it too
    connect(mainWindow, &KTextEditor::MainWindow::viewChanged,
            this,       &LiveSharePluginView::onActiveViewChanged);

    // Guest: host told us to open a file
    connect(m_session, &Session::fileOpenRequested,
            this,      &LiveSharePluginView::onFileOpenRequested);

    onActiveViewChanged(mainWindow->activeView());
}

LiveSharePluginView::~LiveSharePluginView() {
    m_session->stopSession();
    m_session->detachDocument();
    if (m_toolView)
        delete m_toolView;
}

void LiveSharePluginView::onActiveViewChanged(KTextEditor::View *view) {
    if (!view) {
        m_session->detachDocument();
        return;
    }
    // Attach first so the session tracks the new doc
    m_session->attachDocument(view->document(), view);
    // Then broadcast the file switch to all guests (no-op if not hosting)
    m_session->notifyFileOpen(view->document());
}

void LiveSharePluginView::onFileOpenRequested(const QString &url, const QString &content) {
    if (!url.isEmpty()) {
        // Try to open the real file if it exists on this machine too
        QUrl fileUrl(url);
        auto *view = m_mainWindow->openUrl(fileUrl);
        if (view) {
            // Let attachDocument pick it up via viewChanged signal,
            // then overwrite content so guest sees exactly what host has
            m_session->attachDocument(view->document(), view);
            m_session->applyFileContent(content);
            return;
        }
    }
    // Fallback: no URL or file not found — apply content to whatever is open
    m_session->applyFileContent(content);
}

#include "plugin.moc"

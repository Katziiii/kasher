#pragma once

#include "session.h"

#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>
#include <QObject>
#include <QVariantList>

class LiveSharePlugin : public KTextEditor::Plugin {
    Q_OBJECT
public:
    explicit LiveSharePlugin(QObject *parent, const QVariantList &args);
    ~LiveSharePlugin() override = default;

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};

class LiveSharePluginView : public QObject, public KXMLGUIClient {
    Q_OBJECT
public:
    explicit LiveSharePluginView(LiveSharePlugin *plugin,
                                 KTextEditor::MainWindow *mainWindow);
    ~LiveSharePluginView() override;

private Q_SLOTS:
    void onActiveViewChanged(KTextEditor::View *view);
    void onFileOpenRequested(const QString &url, const QString &content);

private:
    KTextEditor::MainWindow *m_mainWindow = nullptr;
    Session                 *m_session    = nullptr;
    QWidget                 *m_toolView   = nullptr;
};

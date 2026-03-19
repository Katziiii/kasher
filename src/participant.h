#pragma once

#include <QColor>
#include <QString>

#include <KTextEditor/Document>
#include <KTextEditor/MovingRange>

struct Participant {
    QString                    id;
    QString                    name;
    QColor                     color;
    int                        line = 0;
    int                        col  = 0;
    KTextEditor::MovingRange  *cursorRange = nullptr;

    void updateCursor(KTextEditor::Document *doc, int newLine, int newCol);
    void detach();
};

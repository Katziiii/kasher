#include "participant.h"

#include <KTextEditor/Attribute>
#include <KTextEditor/Document>

void Participant::updateCursor(KTextEditor::Document *doc, int newLine, int newCol) {
    line = newLine;
    col  = newCol;

    auto *moving = doc;
    if (!moving) return;

    int lineCount = doc->lines();
    if (newLine >= lineCount)
        newLine = qMax(0, lineCount - 1);

    int lineLen = doc->line(newLine).length();
    int endCol  = qMin(newCol + 1, lineLen);
    if (newCol >= lineLen)
        endCol = newCol; // empty-range cursor at end of line

    KTextEditor::Cursor startPos(newLine, newCol);
    KTextEditor::Cursor endPos(newLine, endCol);

    KTextEditor::Attribute::Ptr attr(new KTextEditor::Attribute);
    attr->setBackground(color.lighter(170));
    attr->setForeground(color.darker(140));
    attr->setFontBold(true);
    // Tooltip-like underline so the cursor column is visible even on empty lines
    attr->setFontUnderline(true);

    if (!cursorRange) {
        cursorRange = moving->newMovingRange(
            KTextEditor::Range(startPos, endPos),
            KTextEditor::MovingRange::DoNotExpand,
            KTextEditor::MovingRange::AllowEmpty
        );
        cursorRange->setZDepth(-100.0f);
    } else {
        cursorRange->setRange(KTextEditor::Range(startPos, endPos));
    }
    cursorRange->setAttribute(attr);
}

void Participant::detach() {
    delete cursorRange;
    cursorRange = nullptr;
}

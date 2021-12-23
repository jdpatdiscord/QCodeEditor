// QCodeEditor
#include <internal/QCodeEditor.hpp>
#include <internal/QLineNumberArea.hpp>
#include <internal/QStyleSyntaxHighlighter.hpp>
#include <internal/QSyntaxStyle.hpp>

// Qt
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QCompleter>
#include <QCursor>
#include <QDebug>
#include <QFontDatabase>
#include <QMimeData>
#include <QPaintEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCharFormat>
#include <QTextStream>
#include <QToolTip>

QRegularExpression buildLineStartIndentRegex(int tabSize)
{
    return QRegularExpression("^(\t| {1," + QString::number(tabSize) + "})");
}

QCodeEditor::QCodeEditor(QWidget *widget)
    : QTextEdit(widget), m_highlighter(nullptr), m_syntaxStyle(nullptr), m_lineNumberArea(new QLineNumberArea(this)),
      m_completer(nullptr), m_autoIndentation(true), m_replaceTab(true), m_extraBottomMargin(true),
      m_textChanged(false), m_tabReplace(4, ' '),
      m_parentheses({{'(', ')'}, {'{', '}'}, {'[', ']'}, {'\"', '\"'}, {'\'', '\''}}),
      m_lineStartIndentRegex(buildLineStartIndentRegex(4))
{
    initFont();
    performConnections();
    setMouseTracking(true);

    setSyntaxStyle(QSyntaxStyle::defaultStyle());

    connect(this, &QTextEdit::textChanged, this, [this] {
        if (hasFocus())
            m_textChanged = true;
    });
}

void QCodeEditor::initFont()
{
    auto fnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fnt.setFixedPitch(true);
    fnt.setPointSize(10);

    setFont(fnt);
}

void QCodeEditor::performConnections()
{
    connect(document(), &QTextDocument::blockCountChanged, this, [this]() {
        m_lineNumberArea->updateEditorLineCount();
        updateLineNumberMarginWidth();
    });
    connect(document(), &QTextDocument::blockCountChanged, this, &QCodeEditor::updateBottomMargin);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { m_lineNumberArea->update(); });

    connect(this, &QTextEdit::cursorPositionChanged, this, &QCodeEditor::updateParenthesisAndCurrentLineHighlights);
    connect(this, &QTextEdit::selectionChanged, this, &QCodeEditor::updateWordOccurrenceHighlights);
}

void QCodeEditor::setHighlighter(QStyleSyntaxHighlighter *highlighter)
{
    if (m_highlighter)
    {
        m_highlighter->setDocument(nullptr);
    }

    m_highlighter = highlighter;

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
        m_highlighter->setDocument(document());

        auto comment = m_highlighter->commentLineSequence();
        if (comment.isEmpty())
        {
            m_lineStartCommentRegex = QRegularExpression("");
        }
        else
        {
            m_lineStartCommentRegex = QRegularExpression("^\\s*(" + comment + " ?)");
        }
    }
}

void QCodeEditor::setSyntaxStyle(QSyntaxStyle *style)
{
    m_syntaxStyle = style;

    m_lineNumberArea->setSyntaxStyle(m_syntaxStyle);

    if (m_highlighter)
    {
        m_highlighter->setSyntaxStyle(m_syntaxStyle);
    }

    updateStyle();
}

void QCodeEditor::updateStyle()
{
    if (m_highlighter)
    {
        m_highlighter->rehighlight();
    }

#ifndef QT_NO_STYLE_STYLESHEET
    if (m_syntaxStyle)
    {
        QString backgroundColor = m_syntaxStyle->getFormat("Text").background().color().name();
        QString textColor = m_syntaxStyle->getFormat("Text").foreground().color().name();
        QString selectionBackground = m_syntaxStyle->getFormat("Selection").background().color().name();

        setStyleSheet(QString("QTextEdit { background-color: %1; selection-background-color: %2; color: %3; }")
                          .arg(backgroundColor, selectionBackground, textColor));
    }
#endif

    updateParenthesisAndCurrentLineHighlights();
    updateWordOccurrenceHighlights();
}

void QCodeEditor::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);

    updateLineNumberAreaGeometry();
    updateBottomMargin();
}

void QCodeEditor::changeEvent(QEvent *e)
{
    QTextEdit::changeEvent(e);
    if (e->type() == QEvent::FontChange)
        updateBottomMargin();
}

void QCodeEditor::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() == Qt::ControlModifier)
    {
        const auto sizes = QFontDatabase::standardSizes();
        if (sizes.isEmpty())
        {
            qDebug() << "QFontDatabase::standardSizes() is empty";
            return;
        }
        int newSize = font().pointSize();
        if (e->angleDelta().y() > 0)
            newSize = qMin(newSize + 1, sizes.last());
        else if (e->angleDelta().y() < 0)
            newSize = qMax(newSize - 1, sizes.first());
        if (newSize != font().pointSize())
        {
            QFont newFont = font();
            newFont.setPointSize(newSize);
            setFont(newFont);
            emit fontChanged(newFont);
        }
    }
    else
        QTextEdit::wheelEvent(e);
}

void QCodeEditor::updateLineNumberAreaGeometry()
{
    auto cr = contentsRect();
    cr.setWidth(m_lineNumberArea->width());
    m_lineNumberArea->setGeometry(cr);
}

void QCodeEditor::updateBottomMargin()
{
    auto doc = document();
    if (doc->blockCount() > 1)
    {
        // calling QTextFrame::setFrameFormat with an empty document makes the application crash
        auto rf = doc->rootFrame();
        auto format = rf->frameFormat();
        int documentMargin = doc->documentMargin();
        int bottomMargin = m_extraBottomMargin
                               ? qMax(documentMargin, viewport()->height() - fontMetrics().height()) - documentMargin
                               : documentMargin;
        if (format.bottomMargin() != bottomMargin)
        {
            format.setBottomMargin(bottomMargin);
            rf->setFrameFormat(format);
        }
    }
}

void QCodeEditor::updateLineNumberMarginWidth()
{
    setViewportMargins(m_lineNumberArea->width(), 0, 0, 0);
}

void QCodeEditor::updateLineNumberArea(const QRect &rect)
{
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    updateLineNumberAreaGeometry();

    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberMarginWidth();
    }
}

void QCodeEditor::updateParenthesisAndCurrentLineHighlights()
{
    m_parenAndCurLineHilits.clear();

    highlightCurrentLine();
    highlightParenthesis();

    setExtraSelections(m_parenAndCurLineHilits + m_wordOccurHilits);
}

void QCodeEditor::updateWordOccurrenceHighlights()
{
    m_wordOccurHilits.clear();

    highlightWordOccurrences();

    setExtraSelections(m_parenAndCurLineHilits + m_wordOccurHilits);
}

void QCodeEditor::indent()
{
    static QRegularExpression RE_LINE_START("^");
    addInEachLineOfSelection(RE_LINE_START, m_replaceTab ? m_tabReplace : "\t");
}

void QCodeEditor::unindent()
{
    removeInEachLineOfSelection(m_lineStartIndentRegex, true);
}

void QCodeEditor::swapLineUp()
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();

    if (lineStart == 0)
        return;
    selectionStart -= lines[lineStart - 1].length() + 1;
    selectionEnd -= lines[lineStart - 1].length() + 1;
    lines.move(lineStart - 1, lineEnd);

    cursor.select(QTextCursor::Document);
    cursor.insertText(lines.join('\n'));

    if (cursorAtEnd)
    {
        cursor.setPosition(selectionStart);
        cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(selectionEnd);
        cursor.setPosition(selectionStart, QTextCursor::KeepAnchor);
    }

    setTextCursor(cursor);
}

void QCodeEditor::swapLineDown()
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();

    if (lineEnd == document()->blockCount() - 1)
        return;
    selectionStart += lines[lineEnd + 1].length() + 1;
    selectionEnd += lines[lineEnd + 1].length() + 1;
    lines.move(lineEnd + 1, lineStart);

    cursor.select(QTextCursor::Document);
    cursor.insertText(lines.join('\n'));

    if (cursorAtEnd)
    {
        cursor.setPosition(selectionStart);
        cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(selectionEnd);
        cursor.setPosition(selectionStart, QTextCursor::KeepAnchor);
    }

    setTextCursor(cursor);
}

void QCodeEditor::deleteLine()
{
    auto cursor = textCursor();
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();
    int columnNumber = textCursor().columnNumber();
    cursor.movePosition(QTextCursor::Start);
    if (lineEnd == document()->blockCount() - 1)
    {
        if (lineStart == 0)
        {
            cursor.select(QTextCursor::Document);
        }
        else
        {
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineStart - 1);
            cursor.movePosition(QTextCursor::EndOfBlock);
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        }
    }
    else
    {
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineStart);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, lineEnd - lineStart + 1);
    }
    cursor.removeSelectedText();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor,
                        qMin(columnNumber, cursor.block().text().length()));
    setTextCursor(cursor);
}

void QCodeEditor::duplicate()
{
    auto cursor = textCursor();
    if (cursor.hasSelection()) // duplicate the selection
    {
        auto text = cursor.selectedText();
        bool cursorAtEnd = cursor.selectionEnd() == cursor.position();
        cursor.insertText(text + text);
        if (cursorAtEnd)
        {
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, text.length());
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, text.length());
        }
        else
        {
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, text.length());
        }
    }
    else // duplicate the current line
    {
        int column = cursor.columnNumber();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        auto text = cursor.selectedText();
        cursor.insertText(text + "\n" + text);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, column);
    }
    setTextCursor(cursor);
}

void QCodeEditor::toggleComment()
{
    if (m_highlighter == nullptr)
        return;
    auto comment = m_highlighter->commentLineSequence();
    if (comment.isEmpty())
        return;

    if (!removeInEachLineOfSelection(m_lineStartCommentRegex, false))
    {
        static QRegularExpression RE_LINE_COMMENT_TARGET("\\S|^\\s*$");
        addInEachLineOfSelection(RE_LINE_COMMENT_TARGET, comment + " ");
    }
}

void QCodeEditor::toggleBlockComment()
{
    if (m_highlighter == nullptr)
        return;
    QString commentStart = m_highlighter->startCommentBlockSequence();
    QString commentEnd = m_highlighter->endCommentBlockSequence();

    if (commentStart.isEmpty() || commentEnd.isEmpty())
        return;

    auto cursor = textCursor();
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == endPos;
    auto text = cursor.selectedText();
    int pos1, pos2;
    if (text.indexOf(commentStart) == 0 && text.length() >= commentStart.length() + commentEnd.length() &&
        text.lastIndexOf(commentEnd) + commentEnd.length() == text.length())
    {
        insertPlainText(text.mid(commentStart.length(), text.length() - commentStart.length() - commentEnd.length()));
        pos1 = startPos;
        pos2 = endPos - commentStart.length() - commentEnd.length();
    }
    else
    {
        insertPlainText(commentStart + text + commentEnd);
        pos1 = startPos;
        pos2 = endPos + commentStart.length() + commentEnd.length();
    }
    if (cursorAtEnd)
    {
        cursor.setPosition(pos1);
        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(pos2);
        cursor.setPosition(pos1, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
}

void QCodeEditor::highlightParenthesis()
{
    auto currentSymbol = charUnderCursor();
    auto prevSymbol = charUnderCursor(-1);

    for (auto &p : m_parentheses)
    {
        int direction;

        QChar counterSymbol;
        QChar activeSymbol;
        auto position = textCursor().position();

        if (p.left == currentSymbol)
        {
            direction = 1;
            counterSymbol = p.right;
            activeSymbol = currentSymbol;
        }
        else if (p.right == prevSymbol)
        {
            direction = -1;
            counterSymbol = p.left;
            activeSymbol = prevSymbol;
            position--;
        }
        else
        {
            continue;
        }

        auto counter = 1;

        while (counter != 0 && position > 0 && position < (document()->characterCount() - 1))
        {
            // Moving position
            position += direction;

            auto character = document()->characterAt(position);
            // Checking symbol under position
            if (character == activeSymbol)
            {
                ++counter;
            }
            else if (character == counterSymbol)
            {
                --counter;
            }
        }

        auto format = m_syntaxStyle->getFormat("Parentheses");

        // Found
        if (counter == 0)
        {
            ExtraSelection selection{};

            auto directionEnum = direction < 0 ? QTextCursor::MoveOperation::Left : QTextCursor::MoveOperation::Right;

            // NOTE font weight is not supported in ExtraSelection.
            // See https://doc.qt.io/qt-5/qtextedit-extraselection.html#format-var
            selection.format = format;
            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.setPosition(position);
            selection.cursor.movePosition(QTextCursor::MoveOperation::Right, QTextCursor::MoveMode::KeepAnchor, 1);

            m_parenAndCurLineHilits.append(selection);

            selection.cursor = textCursor();
            selection.cursor.clearSelection();
            selection.cursor.movePosition(directionEnum, QTextCursor::MoveMode::KeepAnchor, 1);

            m_parenAndCurLineHilits.append(selection);
        }

        break;
    }
}

void QCodeEditor::highlightCurrentLine()
{
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection{};

        selection.format = m_syntaxStyle->getFormat("CurrentLine");
        selection.format.setForeground(QBrush());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        m_parenAndCurLineHilits.append(selection);
    }
}

void QCodeEditor::highlightWordOccurrences()
{
    static QRegularExpression RE_WORD(
        R"((?:[_a-zA-Z][_a-zA-Z0-9]*)|(?<=\b|\s|^)(?i)(?:(?:(?:(?:(?:\d+(?:'\d+)*)?\.(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)\.(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)?\.(?:[0-9a-f]+(?:'[0-9a-f]+)*)(?:p[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)\.?(?:p[+-]?(?:\d+(?:'\d+)*))))[lf]?)|(?:(?:(?:[1-9]\d*(?:'\d+)*)|(?:0[0-7]*(?:'[0-7]+)*)|(?:0x[0-9a-f]+(?:'[0-9a-f]+)*)|(?:0b[01]+(?:'[01]+)*))(?:u?l{0,2}|l{0,2}u?)))(?=\b|\s|$))");

    auto curCursor = textCursor();
    if (curCursor.hasSelection())
    {
        auto text = curCursor.selectedText();
        if (RE_WORD.match(text).captured() == text)
        {
            auto doc = document();
            auto wordCursor = doc->find(text, 0, QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
            ExtraSelection e;
            e.format.setBackground(m_syntaxStyle->getFormat("WordOccurrence").background());
            while (!wordCursor.isNull())
            {
                if (wordCursor != curCursor)
                {
                    e.cursor = wordCursor;
                    m_wordOccurHilits.push_back(e);
                }
                wordCursor =
                    doc->find(text, wordCursor, QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
            }
        }
    }
}

void QCodeEditor::paintEvent(QPaintEvent *e)
{
    updateLineNumberArea(e->rect());
    QTextEdit::paintEvent(e);
}

int QCodeEditor::getFirstVisibleBlock()
{
    // Detect the first block for which bounding rect - once translated
    // in absolute coordinated - is contained by the editor's text area

    // Costly way of doing but since "blockBoundingGeometry(...)" doesn't
    // exists for "QTextEdit"...

    auto r1 = viewport()->geometry();
    auto blockTransX = r1.x();
    auto blockTransY = r1.y() - verticalScrollBar()->sliderPosition();
    QTextCursor curs = QTextCursor(document());
    curs.movePosition(QTextCursor::Start);
    auto i = 0;
    for (auto block = document()->begin(); block.isValid(); block = block.next())
    {
        auto r2 = document()->documentLayout()->blockBoundingRect(block).translated(blockTransX, blockTransY).toRect();

        if (r1.intersects(r2))
        {
            return i;
        }

        i++;
    }

    return 0;
}

bool QCodeEditor::proceedCompleterBegin(QKeyEvent *e)
{
    if (m_completer && m_completer->popup()->isVisible())
    {
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return true; // let the completer do default behavior
        default:
            break;
        }
    }

    // todo: Replace with modifiable QShortcut
    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);

    return !(!m_completer || !isShortcut);
}

void QCodeEditor::proceedCompleterEnd(QKeyEvent *e)
{
    auto ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

    if (!m_completer || (ctrlOrShift && e->text().isEmpty()) || e->key() == Qt::Key_Delete)
    {
        return;
    }

    static QString eow(R"(~!@#$%^&*()_+{}|:"<>?,./;'[]\-=)");

    auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
    auto completionPrefix = wordUnderCursor();

    if (!isShortcut && (e->text().isEmpty() || completionPrefix.length() < 2 || eow.contains(e->text().right(1))))
    {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix())
    {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    auto cursRect = cursorRect();
    cursRect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                      m_completer->popup()->verticalScrollBar()->sizeHint().width());

    m_completer->complete(cursRect);
}

void QCodeEditor::keyPressEvent(QKeyEvent *e)
{
    auto completerSkip = proceedCompleterBegin(e);

    if (!completerSkip)
    {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && e->modifiers() != Qt::NoModifier)
        {
            QKeyEvent pureEnter(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
            if (e->modifiers() == Qt::ControlModifier)
            {
                livecodeTrigger();
                return;
            }
            else if (e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
            {
                if (textCursor().blockNumber() == 0)
                {
                    moveCursor(QTextCursor::StartOfBlock);
                    insertPlainText("\n");
                    moveCursor(QTextCursor::PreviousBlock);
                    moveCursor(QTextCursor::EndOfBlock);
                }
                else
                {
                    moveCursor(QTextCursor::PreviousBlock);
                    moveCursor(QTextCursor::EndOfBlock);
                    keyPressEvent(&pureEnter);
                }
                return;
            }
            else if (e->modifiers() == Qt::ShiftModifier)
            {
                keyPressEvent(&pureEnter);
                return;
            }
        }

        if (e->key() == Qt::Key_Tab && e->modifiers() == Qt::NoModifier)
        {
            if (textCursor().hasSelection())
            {
                indent();
                return;
            }

            auto c = charUnderCursor();
            for (auto p : qAsConst(m_parentheses))
            {
                if (p.tabJumpOut && c == p.right)
                {
                    moveCursor(QTextCursor::NextCharacter);
                    return;
                }
            }

            if (m_replaceTab)
            {
                insertPlainText(m_tabReplace);
                return;
            }
        }

        if (e->key() == Qt::Key_Backtab && e->modifiers() == Qt::ShiftModifier)
        {
            unindent();
            return;
        }

        if (e->key() == Qt::Key_Delete && e->modifiers() == Qt::ShiftModifier)
        {
            deleteLine();
            return;
        }

        // Auto indentation

        static QRegularExpression RE_LINE_START_WHITESPACE("^\\s*");

        QString indentationSpaces =
            RE_LINE_START_WHITESPACE.match(document()->findBlockByNumber(textCursor().blockNumber()).text()).captured();

        // Have Qt Edior like behaviour, if {|} and enter is pressed indent the two
        // parenthesis
        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
            e->modifiers() == Qt::NoModifier && charUnderCursor(-1) == '{' && charUnderCursor() == '}')
        {
            insertPlainText("\n" + indentationSpaces + (m_replaceTab ? m_tabReplace : "\t") + "\n" + indentationSpaces);

            for (int i = 0; i <= indentationSpaces.length(); ++i)
                moveCursor(QTextCursor::MoveOperation::Left);

            return;
        }

        // Auto-indent for single "{" without "}"
        if (m_autoIndentation && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
            e->modifiers() == Qt::NoModifier && charUnderCursor(-1) == '{')
        {
            insertPlainText("\n" + indentationSpaces + (m_replaceTab ? m_tabReplace : "\t"));
            setTextCursor(textCursor()); // scroll to the cursor
            return;
        }

        if (e->key() == Qt::Key_Backspace && e->modifiers() == Qt::NoModifier && !textCursor().hasSelection())
        {
            auto pre = charUnderCursor(-1);
            auto nxt = charUnderCursor();
            for (auto p : qAsConst(m_parentheses))
            {
                if (p.autoRemove && p.left == pre && p.right == nxt)
                {
                    auto cursor = textCursor();
                    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                    cursor.removeSelectedText();
                    setTextCursor(textCursor()); // scroll to the cursor
                    return;
                }
            }

            if (textCursor().columnNumber() <= indentationSpaces.length() && textCursor().columnNumber() >= 1 &&
                !m_tabReplace.isEmpty())
            {
                auto cursor = textCursor();
                int realColumn = 0, newIndentLength = 0;
                for (int i = 0; i < cursor.columnNumber(); ++i)
                {
                    if (indentationSpaces[i] != '\t')
                        ++realColumn;
                    else
                    {
                        realColumn =
                            (realColumn + m_tabReplace.length()) / m_tabReplace.length() * m_tabReplace.length();
                    }
                    if (realColumn % m_tabReplace.length() == 0 && i < cursor.columnNumber() - 1)
                    {
                        newIndentLength = i + 1;
                    }
                }
                cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                                    cursor.columnNumber() - newIndentLength);
                cursor.removeSelectedText();
                setTextCursor(textCursor()); // scroll to the cursor
                return;
            }
        }

        for (auto p : qAsConst(m_parentheses))
        {
            if (p.autoComplete)
            {
                auto cursor = textCursor();
                if (cursor.hasSelection())
                {
                    if (p.left == e->text())
                    {
                        // Add parentheses for selection
                        int startPos = cursor.selectionStart();
                        int endPos = cursor.selectionEnd();
                        bool cursorAtEnd = cursor.position() == endPos;
                        auto text = p.left + cursor.selectedText() + p.right;
                        insertPlainText(text);
                        if (cursorAtEnd)
                        {
                            cursor.setPosition(startPos + 1);
                            cursor.setPosition(endPos + 1, QTextCursor::KeepAnchor);
                        }
                        else
                        {
                            cursor.setPosition(endPos + 1);
                            cursor.setPosition(startPos + 1, QTextCursor::KeepAnchor);
                        }
                        setTextCursor(cursor);
                        return;
                    }
                }
                else
                {
                    if (p.right == e->text())
                    {
                        auto symbol = charUnderCursor();

                        if (symbol == p.right)
                        {
                            moveCursor(QTextCursor::NextCharacter);
                            return;
                        }
                    }

                    if (p.left == e->text())
                    {
                        insertPlainText(QString(p.left) + p.right);
                        moveCursor(QTextCursor::PreviousCharacter);
                        return;
                    }
                }
            }
        }

        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && e->modifiers() == Qt::NoModifier)
        {
            insertPlainText("\n" + indentationSpaces.left(textCursor().columnNumber()));
            setTextCursor(textCursor()); // scroll to the cursor
            return;
        }

        if (e->key() == Qt::Key_Escape && textCursor().hasSelection())
        {
            auto cursor = textCursor();
            cursor.clearSelection();
            setTextCursor(cursor);
        }

        QTextEdit::keyPressEvent(e);
    }

    proceedCompleterEnd(e);
}

void QCodeEditor::setAutoIndentation(bool enabled)
{
    m_autoIndentation = enabled;
}

void QCodeEditor::setParentheses(const QVector<Parenthesis> &parentheses)
{
    m_parentheses = parentheses;
}

void QCodeEditor::setExtraBottomMargin(bool enabled)
{
    m_extraBottomMargin = enabled;
    updateBottomMargin();
}

bool QCodeEditor::autoIndentation() const
{
    return m_autoIndentation;
}

void QCodeEditor::setTabReplace(bool enabled)
{
    m_replaceTab = enabled;
}

bool QCodeEditor::tabReplace() const
{
    return m_replaceTab;
}

void QCodeEditor::setTabReplaceSize(int val)
{
    m_tabReplace.fill(' ', val);
    m_lineStartIndentRegex = buildLineStartIndentRegex(val);
#if QT_VERSION >= 0x050B00
    setTabStopDistance(fontMetrics().horizontalAdvance(QString(val * 1000, ' ')) / 1000.0);
#elif QT_VERSION == 0x050A00
    setTabStopDistance(fontMetrics().width(QString(val * 1000, ' ')) / 1000.0);
#else
    setTabStopWidth(fontMetrics().width(QString(val * 1000, ' ')) / 1000.0);
#endif
}

int QCodeEditor::tabReplaceSize() const
{
    return m_tabReplace.size();
}

void QCodeEditor::setCompleter(QCompleter *completer)
{
    if (m_completer)
    {
        disconnect(m_completer, nullptr, this, nullptr);
    }

    m_completer = completer;

    if (!m_completer)
    {
        return;
    }

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);

    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this, &QCodeEditor::insertCompletion);
}

void QCodeEditor::focusInEvent(QFocusEvent *e)
{
    if (m_completer)
    {
        m_completer->setWidget(this);
    }

    m_textChanged = false;
    QTextEdit::focusInEvent(e);
}

void QCodeEditor::focusOutEvent(QFocusEvent *e)
{
    QTextEdit::focusOutEvent(e);

    if (m_textChanged)
    {
        m_textChanged = false;
        emit editingFinished();
    }
}

bool QCodeEditor::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto helpEvent = dynamic_cast<QHelpEvent *>(event);
        auto point = helpEvent->pos();
        point.setX(point.x() - m_lineNumberArea->geometry().right());
        auto pos = cursorForPosition(point).position();

        QString text;
        m_diagSpans.overlap_find_all({pos, pos}, [&text, this](auto it) {
            const auto &diag = m_diagnostics[it->interval().diagIndex];
            if (!text.isEmpty())
            {
                text += "<hr>";
            }
            // NOTE <nobr> does not work in QToolTip. See
            // https://doc.qt.io/qt-5/qtooltip.html#details
            text += "<p style=\"margin: 0; white-space:pre\">";
            text += diag.message.toHtmlEscaped();
            if (!diag.code.isEmpty())
            {
                text += "  <font color=\"";
                switch (diag.severity)
                {
                case DiagnosticSeverity::Hint:
                    text += m_syntaxStyle->getFormat("Text").foreground().color().name();
                    break;
                case DiagnosticSeverity::Information:
                    text += m_syntaxStyle->getFormat("Information").underlineColor().name();
                    break;
                case DiagnosticSeverity::Warning:
                    text += m_syntaxStyle->getFormat("Warning").underlineColor().name();
                    break;
                case DiagnosticSeverity::Error:
                    text += m_syntaxStyle->getFormat("Error").underlineColor().name();
                    break;
                }
                text += "\"><small>";
                text += diag.code.toHtmlEscaped();
                text += "</small></font>";
            }
            text += "</p>";
            return true;
        });

        if (text.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(helpEvent->globalPos(), text);

        return true;
    }
    return QTextEdit::event(event);
}

void QCodeEditor::insertCompletion(const QString &s)
{
    if (m_completer->widget() != this)
    {
        return;
    }

    auto tc = textCursor();
    tc.select(QTextCursor::SelectionType::WordUnderCursor);
    tc.insertText(s);
    setTextCursor(tc);
}

QCompleter *QCodeEditor::completer() const
{
    return m_completer;
}

void QCodeEditor::addDiagnostic(DiagnosticSeverity severity, const Span &span, const QString &message,
                                const QString &code)
{
    if (span.end < span.start)
        return;

    auto i = m_diagnostics.size();
    m_diagnostics.push_back(Diagnostic(severity, span, message, code));
    m_diagSpans.insert(InternalSpan(span.start, span.end, i));

    auto cursor = this->textCursor();
    cursor.setPosition(span.start);
    cursor.setPosition(span.end, QTextCursor::KeepAnchor);

    QTextCharFormat charfmt;

    switch (severity)
    {
    case DiagnosticSeverity::Error:
        charfmt.setUnderlineColor(m_syntaxStyle->getFormat("Error").underlineColor());
        charfmt.setUnderlineStyle(m_syntaxStyle->getFormat("Error").underlineStyle());
        break;
    case DiagnosticSeverity::Warning:
        charfmt.setUnderlineColor(m_syntaxStyle->getFormat("Warning").underlineColor());
        charfmt.setUnderlineStyle(m_syntaxStyle->getFormat("Warning").underlineStyle());
        break;
    case DiagnosticSeverity::Information:
        charfmt.setUnderlineColor(m_syntaxStyle->getFormat("Information").underlineColor());
        charfmt.setUnderlineStyle(m_syntaxStyle->getFormat("Information").underlineStyle());
        break;
    case DiagnosticSeverity::Hint:
        charfmt.setUnderlineColor(m_syntaxStyle->getFormat("Text").foreground().color());
        charfmt.setUnderlineStyle(QTextCharFormat::DotLine);
        break;
    }

    cursor.mergeCharFormat(charfmt);

    cursor.setPosition(span.start);
    auto startLine = cursor.blockNumber();
    cursor.setPosition(span.end);
    auto endLine = cursor.blockNumber();
    m_lineNumberArea->addDiagnosticMarker(severity, startLine, endLine + 1);
}

void QCodeEditor::clearDiagnostics()
{
    if (m_diagnostics.empty())
        return;

    m_diagnostics.clear();
    m_diagSpans.clear();

    QTextCharFormat charfmt;
    charfmt.setUnderlineStyle(QTextCharFormat::NoUnderline);

    auto cursor = textCursor();
    cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(charfmt);

    m_lineNumberArea->clearDiagnosticMarkers();

    update();
}

QChar QCodeEditor::charUnderCursor(int offset) const
{
    return document()->characterAt(textCursor().position() + offset);
}

QString QCodeEditor::wordUnderCursor() const
{
    auto tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

void QCodeEditor::insertFromMimeData(const QMimeData *source)
{
    insertPlainText(source->text());
}

bool QCodeEditor::removeInEachLineOfSelection(const QRegularExpression &regex, bool force)
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();
    QString newText;
    QTextStream stream(&newText);
    int deleteTotal = 0, deleteFirst = 0;
    for (int i = lineStart; i <= lineEnd; ++i)
    {
        auto line = lines[i];
        auto match = regex.match(line).captured(1);
        int len = match.length();
        if (len == 0 && !force)
            return false;
        if (i == lineStart)
            deleteFirst = len;
        deleteTotal += len;
        stream << line.remove(line.indexOf(match), len);
        if (i != lineEnd)
#if QT_VERSION >= 0x50E00
            stream << Qt::endl;
#else
            stream << endl;
#endif
    }
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineStart);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, lineEnd - lineStart);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(newText);
    cursor.setPosition(qMax(0, selectionStart - deleteFirst));
    if (cursor.blockNumber() < lineStart)
    {
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineStart - cursor.blockNumber());
        cursor.movePosition(QTextCursor::StartOfBlock);
    }
    int pos = cursor.position();
    cursor.setPosition(selectionEnd - deleteTotal);
    if (cursor.blockNumber() < lineEnd)
    {
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineEnd - cursor.blockNumber());
        cursor.movePosition(QTextCursor::StartOfBlock);
    }
    int pos2 = cursor.position();
    if (cursorAtEnd)
    {
        cursor.setPosition(pos);
        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(pos2);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
    return true;
}

void QCodeEditor::addInEachLineOfSelection(const QRegularExpression &regex, const QString &str)
{
    auto cursor = textCursor();
    auto lines = toPlainText().remove('\r').split('\n');
    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();
    bool cursorAtEnd = cursor.position() == selectionEnd;
    cursor.setPosition(selectionStart);
    int lineStart = cursor.blockNumber();
    cursor.setPosition(selectionEnd);
    int lineEnd = cursor.blockNumber();
    QString newText;
    QTextStream stream(&newText);
    for (int i = lineStart; i <= lineEnd; ++i)
    {
        auto line = lines[i];
        stream << line.insert(line.indexOf(regex), str);
        if (i != lineEnd)
#if QT_VERSION >= 0x50E00
            stream << Qt::endl;
#else
            stream << endl;
#endif
    }
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineStart);
    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, lineEnd - lineStart);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(newText);
    int pos = selectionStart + str.length();
    int pos2 = selectionEnd + str.length() * (lineEnd - lineStart + 1);
    if (cursorAtEnd)
    {
        cursor.setPosition(pos);
        cursor.setPosition(pos2, QTextCursor::KeepAnchor);
    }
    else
    {
        cursor.setPosition(pos2);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
}

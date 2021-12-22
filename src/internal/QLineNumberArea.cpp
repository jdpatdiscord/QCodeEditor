// QCodeEditor
#include <internal/QCodeEditor.hpp>
#include <internal/QLineNumberArea.hpp>
#include <internal/QSyntaxStyle.hpp>

// Qt
#include <QAbstractTextDocumentLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>

QLineNumberArea::QLineNumberArea(QCodeEditor *parent)
    : QWidget(parent), m_syntaxStyle(nullptr), m_codeEditParent(parent)
{
}

QSize QLineNumberArea::sizeHint() const
{
    if (m_codeEditParent == nullptr)
    {
        return QWidget::sizeHint();
    }

    const int digits = QString::number(m_codeEditParent->document()->blockCount()).length();
    int space;

#if QT_VERSION >= 0x050B00
    space = 15 + m_codeEditParent->fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
#else
    space = 15 + m_codeEditParent->fontMetrics().width(QLatin1Char('9')) * digits;
#endif

    return {space, 0};
}

void QLineNumberArea::setSyntaxStyle(QSyntaxStyle *style)
{
    m_syntaxStyle = style;
}

QSyntaxStyle *QLineNumberArea::syntaxStyle() const
{
    return m_syntaxStyle;
}

void QLineNumberArea::addDiagnosticMarker(QCodeEditor::DiagnosticSeverity severity, int startLine, int endLine)
{
    for (int i = startLine; i < endLine; ++i)
    {
        auto j = m_diagnosticMarkers.find(i);
        if (j == m_diagnosticMarkers.end())
        {
            m_diagnosticMarkers.insert(i, severity);
        }
        else
        {
            *j = qMax(*j, severity);
        }
    }
    update();
}

void QLineNumberArea::clearDiagnosticMarkers()
{
    m_diagnosticMarkers.clear();
    update();
}

void QLineNumberArea::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    // Clearing rect to update
    painter.fillRect(event->rect(), m_syntaxStyle->getFormat("LineNumber").background().color());

    auto blockNumber = m_codeEditParent->getFirstVisibleBlock();
    auto block = m_codeEditParent->document()->findBlockByNumber(blockNumber);
    auto top = (int)m_codeEditParent->document()
                   ->documentLayout()
                   ->blockBoundingRect(block)
                   .translated(0, -m_codeEditParent->verticalScrollBar()->value())
                   .top();
    auto bottom = top + (int)m_codeEditParent->document()->documentLayout()->blockBoundingRect(block).height();

    auto currentLineFormat = m_syntaxStyle->getFormat("CurrentLineNumber");
    auto currentLine = currentLineFormat.foreground().color();
    auto otherLines = m_syntaxStyle->getFormat("LineNumber").foreground().color();

    auto font = m_codeEditParent->font();
    QFont currentLineFont(font);
    currentLineFont.setWeight(currentLineFormat.fontWeight());
    currentLineFont.setItalic(currentLineFormat.fontItalic());
    painter.setFont(font);

    auto lineWidth = sizeHint().width();
    auto lineHeight = m_codeEditParent->fontMetrics().height();

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);

            if (m_diagnosticMarkers.contains(blockNumber))
            {
                QColor markerColor;
                switch (m_diagnosticMarkers[blockNumber])
                {
                case QCodeEditor::DiagnosticSeverity::Error:
                    markerColor = m_syntaxStyle->getFormat("Error").underlineColor();
                    break;
                case QCodeEditor::DiagnosticSeverity::Warning:
                    markerColor = m_syntaxStyle->getFormat("Warning").underlineColor();
                    break;
                case QCodeEditor::DiagnosticSeverity::Information:
                    markerColor = m_syntaxStyle->getFormat("Information").underlineColor();
                    break;
                case QCodeEditor::DiagnosticSeverity::Hint:
                    markerColor = m_syntaxStyle->getFormat("Text").foreground().color();
                    break;
                default:
                    Q_UNREACHABLE();
                    break;
                }

                painter.fillRect(0, top, 7, lineHeight, markerColor);
            }

            auto isCurrentLine = m_codeEditParent->textCursor().blockNumber() == blockNumber;
            painter.setPen(isCurrentLine ? currentLine : otherLines);

            if (isCurrentLine)
            {
                painter.setFont(currentLineFont);
                painter.drawText(-5, top, lineWidth, lineHeight, Qt::AlignRight, number);
                painter.setFont(font);
            }
            else
            {
                painter.drawText(-5, top, lineWidth, lineHeight, Qt::AlignRight, number);
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)m_codeEditParent->document()->documentLayout()->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

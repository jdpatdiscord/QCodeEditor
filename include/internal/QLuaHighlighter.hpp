#pragma once

// QCodeEditor
#include <internal/QStyleSyntaxHighlighter.hpp> // Required for inheritance
#include <internal/QHighlightBlockRule.hpp>
#include <internal/QHighlightRule.hpp>

// Qt
#include <QMap>
#include <QRegularExpression>
#include <QVector>

class QString;
class QTextDocument;
class QSyntaxStyle;

/**
 * @brief Class, that describes Lua code
 * highlighter.
 */
class QLuaHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
  public:
    /**
     * @brief Constructor.
     * @param document Pointer to document.
     */
    explicit QLuaHighlighter(QTextDocument *document = nullptr);

  protected:
    void highlightBlock(const QString &text) override;

  private:
    QVector<QHighlightRule> m_highlightRules;
    QVector<QHighlightBlockRule> m_highlightBlockRules;

    QRegularExpression m_requirePattern;
    QRegularExpression m_functionPattern;
    QRegularExpression m_defTypePattern;
};

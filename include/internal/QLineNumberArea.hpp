#pragma once

// Qt
#include <QWidget> // Required for inheritance

#include <QCodeEditor>

class QSyntaxStyle;

/**
 * @brief Class, that describes line number area widget.
 */
class QLineNumberArea : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor.
     * @param parent Pointer to parent QTextEdit widget.
     */
    explicit QLineNumberArea(QCodeEditor *parent = nullptr);

    // Disable copying
    QLineNumberArea(const QLineNumberArea &) = delete;
    QLineNumberArea &operator=(const QLineNumberArea &) = delete;

    /**
     * @brief Overridden method for getting line number area
     * size.
     */
    QSize sizeHint() const override;

    /**
     * @brief Method for setting syntax style object.
     * @param style Pointer to syntax style.
     */
    void setSyntaxStyle(QSyntaxStyle *style);

    /**
     * @brief Method for getting syntax style.
     * @return Pointer to syntax style.
     */
    QSyntaxStyle *syntaxStyle() const;

    /**
     * @brief Add a marker to the line number area, indicating the severity of
     *        diagnostics in a line. If there is already a marker at the same line,
     *        the higher severity will take priority.
     * 
     * @param startLine 0-indexed.
     * @param endLine 0-indexed.
     */
    void addDiagnosticMarker(QCodeEditor::DiagnosticSeverity severity, int startLine, int endLine);

    void clearDiagnosticMarkers();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QSyntaxStyle *m_syntaxStyle;

    QCodeEditor *m_codeEditParent;

    QMap<int, QCodeEditor::DiagnosticSeverity> m_diagnosticMarkers;
};

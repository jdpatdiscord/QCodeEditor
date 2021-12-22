#include <DiagnosticListItem.hpp>
#include <QApplication>
#include <QStyle>

DiagnosticListItem::DiagnosticListItem(QCodeEditor::DiagnosticSeverity severity, const QCodeEditor::Span &span,
                                       const QString &code, const QString &message)
    : QListWidgetItem(nullptr, QListWidgetItem::UserType), m_severity(severity), m_code(code), m_message(message), m_span(span)
{
    switch (severity)
    {
    case QCodeEditor::DiagnosticSeverity::Hint:
        break;
    case QCodeEditor::DiagnosticSeverity::Information:
        setIcon(qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation));
        break;
    case QCodeEditor::DiagnosticSeverity::Warning:
        setIcon(qApp->style()->standardIcon(QStyle::SP_MessageBoxWarning));
        break;
    case QCodeEditor::DiagnosticSeverity::Error:
        setIcon(qApp->style()->standardIcon(QStyle::SP_MessageBoxCritical));
        break;
    }
    setText(code + " " + message);
}
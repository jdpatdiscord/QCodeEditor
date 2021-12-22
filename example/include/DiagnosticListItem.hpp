#pragma once

#include <QCodeEditor>
#include <QListWidgetItem>
#include <QString>

class DiagnosticListItem : public QListWidgetItem
{
  public:
    DiagnosticListItem(QCodeEditor::DiagnosticSeverity severity, const QCodeEditor::Span &span, const QString &code,
                       const QString &message);

  public:
    QCodeEditor::DiagnosticSeverity m_severity;
    QString m_code;
    QString m_message;
    QCodeEditor::Span m_span;
};
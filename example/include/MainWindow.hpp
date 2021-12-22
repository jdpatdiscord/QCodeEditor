#pragma once

// Qt
#include <QAction>
#include <QMainWindow> // Required for inheritance
#include <QMenu>
#include <QMenuBar>
#include <QPair>
#include <QString>
#include <QVector>

class QLineEdit;
class QVBoxLayout;
class QSyntaxStyle;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QCompleter;
class QStyleSyntaxHighlighter;
class QCodeEditor;
class QListWidget;

/**
 * @brief Class, that describes demo main window.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor.
     * @param parent Pointer to parent widget.
     */
    explicit MainWindow(QWidget *parent = nullptr);

  private slots:
    void addDiagnostic();
    void removeDiagnostic();

  private:
    void loadStyle(QString path);

    QString loadCode(QString path);

    void initData();

    void createWidgets();

    void setupWidgets();

    void performConnections();

    void updateDiagnostics();

    QVBoxLayout *m_setupLayout;

    QComboBox *m_codeSampleCombobox;
    QComboBox *m_highlighterCombobox;
    QComboBox *m_completerCombobox;
    QComboBox *m_styleCombobox;

    QCheckBox *m_readOnlyCheckBox;
    QCheckBox *m_wordWrapCheckBox;
    QCheckBox *m_tabReplaceEnabledCheckbox;
    QSpinBox *m_tabReplaceNumberSpinbox;
    QCheckBox *m_autoIndentationCheckbox;

    QMenu *m_mainMenu;
    QAction *m_actionToggleComment;
    QAction *m_actionToggleBlockComment;

    QCodeEditor *m_codeEditor;

    QListWidget *m_diagnostics;
    QLineEdit *m_diagCode;
    QLineEdit *m_diagMessage;
    int m_diagSeverity;

    QVector<QPair<QString, QString>> m_codeSamples;
    QVector<QPair<QString, QCompleter *>> m_completers;
    QVector<QPair<QString, QStyleSyntaxHighlighter *>> m_highlighters;
    QVector<QPair<QString, QSyntaxStyle *>> m_styles;
};

#pragma once

// Qt
#include <QCompleter> // Required for inheritance

/**
 * @brief Class, that describes completer with
 * Python specific types and functions.
 */
class QPythonCompleter : public QCompleter
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor.
     * @param parent Pointer to parent QObject.
     */
    explicit QPythonCompleter(QObject *parent = nullptr);
};

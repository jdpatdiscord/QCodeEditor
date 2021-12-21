#pragma once

// Qt
#include <QRegularExpression>
#include <QString>

struct QHighlightRule
{
    QHighlightRule()
    {
    }

    QHighlightRule(QRegularExpression p, QString f) : pattern(std::move(p)), formatName(std::move(f))
    {
    }

    QRegularExpression pattern;
    QString formatName;
};
#pragma once

#include <utility>

// Qt
#include <QRegularExpression>
#include <QString>

struct QHighlightBlockRule
{
    QHighlightBlockRule()
    {
    }

    QHighlightBlockRule(QRegularExpression start, QRegularExpression end, QString format)
        : startPattern(std::move(start)), endPattern(std::move(end)), formatName(std::move(format))
    {
    }

    QRegularExpression startPattern;
    QRegularExpression endPattern;
    QString formatName;
};

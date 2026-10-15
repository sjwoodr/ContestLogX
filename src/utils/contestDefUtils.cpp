/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "contestDefUtils.h"

#include <QJsonArray>

namespace {

// Mirrors MainWindow::isFieldVisible: a qsoFields column with a visibleWhen
// is shown only when the named prompt's answer is in its values list.
bool columnVisible(const QJsonObject& contestDef, const QString& column,
                   const ContestDefUtils::PromptLookup& promptValue)
{
    for (const QJsonValue& v : contestDef["qsoFields"].toArray()) {
        const QJsonObject field = v.toObject();
        if (field["column"].toString() != column || !field.contains("visibleWhen"))
            continue;
        const QJsonObject vw = field["visibleWhen"].toObject();
        const QString actual = promptValue ? promptValue(vw["promptId"].toString()) : QString();
        for (const QJsonValue& allowed : vw["values"].toArray()) {
            if (allowed.toString() == actual)
                return true;
        }
        return false;
    }
    return true;
}

} // namespace

bool ContestDefUtils::sendsSerialNumber(const QJsonObject& contestDef,
                                        const QString& stationClass,
                                        const PromptLookup& promptValue)
{
    if (!stationClass.isEmpty()) {
        for (const QJsonValue& v : contestDef["stationClasses"].toObject()["classes"].toArray()) {
            const QJsonObject cls = v.toObject();
            if (cls["id"].toString() == stationClass
                    && cls["exchangeSent"].toObject()["type"].toString() == "serial")
                return true;
        }
    }

    for (const QJsonValue& v : contestDef["exchangeFields"].toObject()["sent"].toArray()) {
        const QJsonObject field = v.toObject();
        if (field["type"].toString() == "serial")
            return columnVisible(contestDef, field["name"].toString(), promptValue);
    }
    return false;
}

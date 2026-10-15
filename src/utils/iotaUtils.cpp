/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#include "iotaUtils.h"

#include <QRegularExpression>

QString IotaUtils::normalizeRef(const QString& ref)
{
    static const QRegularExpression re(
        QStringLiteral("^(AF|AN|AS|EU|NA|OC|SA)-?([0-9]{1,3})$"));
    const QRegularExpressionMatch m = re.match(ref.trimmed().toUpper());
    if (!m.hasMatch())
        return QString();
    const int number = m.captured(2).toInt();
    if (number < 1)
        return QString();
    return QStringLiteral("%1-%2").arg(m.captured(1)).arg(number, 3, 10, QLatin1Char('0'));
}

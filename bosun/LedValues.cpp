// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LedValues.h"

#include <QObject>

namespace bosun {

std::optional<QList<int>> checkedLedValues(const QVariantList &values, QString *why)
{
    constexpr qsizetype Group = 4;
    constexpr int LastLed = 145;
    constexpr qsizetype MostGroups = 146;
    if (values.isEmpty() || values.size() % Group != 0 || values.size() / Group > MostGroups) {
        *why = QObject::tr("A list of LEDs has one to 146 groups of four numbers");
        return std::nullopt;
    }
    QList<int> list;
    list.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i) {
        const QVariant &item = values.at(i);
        // A whole number only: not "7", not 7.5. QML hands over numbers as doubles, so a whole double is fine.
        bool ok = false;
        const double number = item.toDouble(&ok);
        const bool isNumber = ok && item.typeId() != QMetaType::QString;
        const int value = static_cast<int>(number);
        const bool ledPlace = i % Group == 0;
        if (!isNumber || number != static_cast<double>(value) || value < 0 || value > (ledPlace ? LastLed : 255)) {
            *why = ledPlace ? QObject::tr("A LED is numbered from 0 to 145") : QObject::tr("A colour has three values from 0 to 255");
            return std::nullopt;
        }
        list << value;
    }
    return list;
}

} // namespace bosun

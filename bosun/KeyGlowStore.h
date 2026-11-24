// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KeyGlowAccess.h>

#include <QObject>
#include <QString>

namespace bosun {

// The choice "the keys glow under your fingers", as the settings window holds it: OFF unless the person made it. This effect reads the key presses of the
// keyboard (inside galleond only), so it is the choice of AGENTS.md rule 5 (explicit activation, state shown): read from key-glow.json when the window starts (a
// missing or invalid file means off) and written back at each change, atomically; galleond reads the file and never writes it, and no program can change it.
// An empty path (a preview run) keeps the choice in memory and writes nothing.
//
// `supported` is false where galleond has no source of key presses yet (Windows, for now): the window then shows nothing about it, so that it never offers
// a choice that does nothing.
class KeyGlowStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool allow READ allow NOTIFY changed)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(QString colour READ colour NOTIFY changed) // "#rrggbb": the colour of the glow, cyan until the person chooses another

public:
    explicit KeyGlowStore(QString path = {}, bool supported = defaultSupported(), QObject *parent = nullptr);

    [[nodiscard]] bool allow() const { return m_choice.allow; }
    [[nodiscard]] bool supported() const { return m_supported; }
    [[nodiscard]] QString colour() const;
    // True where galleond has a source of key presses: Linux.
    [[nodiscard]] static constexpr bool defaultSupported()
    {
#ifdef Q_OS_WIN
        return false;
#else
        return true;
#endif
    }

    // "" when done, else why not (the choice in memory is kept even if the file could not be written).
    Q_INVOKABLE [[nodiscard]] QString setAllow(bool allow);
    // The colour of the glow, written as #rrggbb; "" when done, else why not (a text that is not a colour changes nothing). Written with the rest of the choice.
    Q_INVOKABLE [[nodiscard]] QString setColour(const QString &colour);

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString save();

    QString m_path;
    galleond::KeyGlowChoice m_choice;
    bool m_supported = true;
};

} // namespace bosun

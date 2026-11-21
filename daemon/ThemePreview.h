// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace galleond {

// Bosun shows the module on the theme of the tab that is open, and gives the module back when the person leaves
// the theme tabs. This is the memory and the decisions of that preview, pure: the pages and the module are
// the caller's. The page the module showed first is remembered once, however many tabs follow.
class ThemePreview
{
public:
    // The page of a theme that no longer exists, or the page of a game that has ended, is replaced by this one.
    static constexpr const char *FallbackPage = "numpad";

    enum class Kind : unsigned char {
        Nothing,   // leave the module as it is (nothing remembered, or the module was changed since)
        Show,      // show `page` again
        AfterGame, // a game runs: `page` is what the module goes back to when the game ends
    };
    struct Revert
    {
        Kind kind = Kind::Nothing;
        QString page;
    };

    // The module shows `current` and is asked to show `shown`. Refused (false) while a game runs, and for an empty page.
    bool begin(const QString &current, const QString &shown, bool gameActive);

    // The preview ends. `current` is the page the module shows now; `themePages` the pages of the themes there are.
    // The preview is forgotten.
    [[nodiscard]] Revert end(const QString &current, bool gameActive, const QStringList &themePages);

    // Forgets the preview without giving anything back (a theme was shown for good).
    void clear();

    [[nodiscard]] bool active() const { return !m_shown.isEmpty(); }

private:
    QString m_before; // what the module showed before the first preview ("" when it showed no theme)
    QString m_shown;  // the last page shown by a preview ("" when there is no preview)
};

} // namespace galleond

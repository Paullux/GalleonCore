// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemePreview.h"

namespace galleond {

bool ThemePreview::begin(const QString &current, const QString &shown, bool gameActive)
{
    if (gameActive || shown.isEmpty())
        return false;
    if (m_shown.isEmpty())
        m_before = current; // the first tab remembers the way back, the next ones keep it
    m_shown = shown;
    return true;
}

ThemePreview::Revert ThemePreview::end(const QString &current, bool gameActive, const QStringList &themePages)
{
    const QString before = m_before;
    const QString shown = m_shown;
    clear();
    // Nothing remembered, or the module no longer shows what the preview put there (a dial, a player that came
    // or went): that was a choice made since, it is not undone.
    if (shown.isEmpty() || before.isEmpty() || current != shown)
        return {};
    // The page before may be gone (a theme removed) or have been a game's, which does not outlive the game: the numpad.
    QString target = before;
    if (!themePages.contains(target))
        target = themePages.contains(QString::fromLatin1(FallbackPage)) ? QString::fromLatin1(FallbackPage) : QString();
    if (target.isEmpty())
        return {};
    return {gameActive ? Kind::AfterGame : Kind::Show, target};
}

void ThemePreview::clear()
{
    m_before.clear();
    m_shown.clear();
}

} // namespace galleond

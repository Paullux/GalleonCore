// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// Pure geometry: keep the selected title whole, sharing any shortage equally
// between the other tabs. Tabs that reach the minimum leave the remaining pool.
function distribute(preferred, selected, available, spacing, minimum) {
    const widths = preferred.map(width => Math.max(minimum, width))
    if (widths.length === 0) return widths
    const budget = Math.max(0, available - spacing * (widths.length - 1))
    const total = widths.reduce((sum, width) => sum + width, 0)
    if (total <= budget)
        return widths.map(width => width + (budget - total) / widths.length)

    let shortage = total - budget
    let shrinking = widths.map((width, index) => index).filter(index => index !== selected && widths[index] > minimum)
    while (shortage > 0.01 && shrinking.length > 0) {
        const share = shortage / shrinking.length
        for (const index of shrinking) {
            const reduction = Math.min(share, widths[index] - minimum)
            widths[index] -= reduction
            shortage -= reduction
        }
        shrinking = shrinking.filter(index => widths[index] > minimum + 0.01)
    }
    return widths
}

// The narrowest an unselected tab may be so that every tab stays in view: the selected title stays whole and the others share what is left,
// but never wider than `ceiling` (the usual minimum, kept when everything fits) and never narrower than `floor` (a colour bar and a few
// letters). It follows the measured width of the titles, so it follows the language: German and Spanish titles are longer than English ones.
// Below `floor` the tabs are not shortened further and the bar scrolls.
function fitMinimum(preferred, selected, available, spacing, ceiling, floor) {
    const others = preferred.length - 1
    if (others < 1) return ceiling
    const kept = selected >= 0 && selected < preferred.length ? Math.max(ceiling, preferred[selected]) : ceiling
    const room = (available - spacing * others - kept) / others
    return Math.max(floor, Math.min(ceiling, Math.floor(room)))
}

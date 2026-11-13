// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BrightnessScheduler.h"

#include "KeyboardController.h"

#include <QDebug>

namespace galleond {

BrightnessScheduler::BrightnessScheduler(KeyboardController &keyboard, const QString &curvePath, Clock clock, int tickMs, QObject *parent)
    : QObject(parent), m_keyboard(keyboard), m_path(curvePath), m_clock(std::move(clock)), m_watcher(curvePath)
{
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, &BrightnessScheduler::reload);
    connect(&keyboard, &KeyboardController::brightnessRead, this, [this](int value, bool ok, const QString &) { onRead(value, ok); });
    connect(&keyboard, &KeyboardController::brightnessWritten, this, [this](int value, bool ok, const QString &) { onWritten(value, ok); });
    m_tick.setInterval(tickMs);
    connect(&m_tick, &QTimer::timeout, this, &BrightnessScheduler::evaluate);
    m_tick.start();
    reload();
}

bool BrightnessScheduler::active() const
{
    return m_curve && m_curve->mode == BrightnessCurve::Mode::Curve;
}

void BrightnessScheduler::reload()
{
    const auto fresh = readBrightnessCurve(m_path);
    if (!fresh) {
        // A missing or invalid file: the curve in force stays if there is one (a half-written or damaged file
        // never switches the curve off), and nothing starts from nothing.
        return;
    }
    const bool wasActive = active();
    m_curve = fresh;
    if (!active()) {
        // The slider is in charge again: forget what was applied, so switching the curve back on starts afresh.
        m_applied.reset();
        m_target.reset();
        m_awaitingRead = false;
        m_awaitingWrite = false;
        return;
    }
    if (!wasActive)
        m_applied.reset(); // just switched on: check the keyboard against the current point
    evaluate();
}

void BrightnessScheduler::evaluate()
{
    if (!m_curve || m_curve->mode != BrightnessCurve::Mode::Curve || m_awaitingRead || m_awaitingWrite)
        return;
    const int target = brightnessAt(*m_curve, m_clock());
    if (m_applied == target)
        return; // already holds this hour's point (by our doing): never rewritten
    m_target = target;
    m_awaitingRead = true;
    static_cast<void>(m_keyboard.ReadBrightness()); // read first: write only if the keyboard differs
}

void BrightnessScheduler::onRead(int value, bool ok)
{
    if (!m_awaitingRead)
        return; // a read asked by someone else (Bosun's slider, the state file): not ours
    m_awaitingRead = false;
    if (!ok || !m_target) {
        qWarning() << "galleond: brightness curve: could not read the keyboard, nothing written; trying again at the next tick";
        return;
    }
    if (!active()) // switched off while the read was in flight
        return;
    if (value == *m_target) {
        m_applied = m_target; // the keyboard already holds it: no write
        return;
    }
    m_awaitingWrite = true;
    if (!m_keyboard.SetBrightness(*m_target)) {
        m_awaitingWrite = false;
        qWarning() << "galleond: brightness curve: the keyboard component refused" << *m_target;
    }
}

void BrightnessScheduler::onWritten(int value, bool ok)
{
    if (!m_awaitingWrite || !m_target || value != *m_target)
        return; // a write by someone else (the slider): the curve does not touch it
    m_awaitingWrite = false;
    if (ok && active())
        m_applied = m_target;
    else if (!ok)
        qWarning() << "galleond: brightness curve: the write of" << value << "was not confirmed; trying again at the next tick";
}

} // namespace galleond

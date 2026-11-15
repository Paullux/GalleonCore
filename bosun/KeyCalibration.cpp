// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyCalibration.h"

#include <KeyMapFile.h>

#include <array>

#include <galleon/keyboard/KeyIdentity.h>
#include <galleon/keyboard/LiveFrame.h>

namespace bosun {

namespace {

using galleon::keyboard::AnswerOutcome;

constexpr int LitRed = 255; // the LED being asked about: plain white, the easiest to see
constexpr int LitGreen = 255;
constexpr int LitBlue = 255;
constexpr int RestRed = 90; // the colour the lighting starts with when calibrating is over
constexpr int RestGreen = 90;
constexpr int RestBlue = 90;

constexpr galleon::keyboard::QtScanCodeSystem nativeSystem()
{
#ifdef Q_OS_WIN
    return galleon::keyboard::QtScanCodeSystem::Windows;
#else
    return galleon::keyboard::QtScanCodeSystem::Linux;
#endif
}

} // namespace

KeyCalibration::KeyCalibration(CalibrationLights &lights, QString mapPath, QObject *parent)
    : QObject(parent), m_lights(lights), m_path(std::move(mapPath))
{
}

KeyCalibration::~KeyCalibration() = default;

int KeyCalibration::currentLed() const
{
    const auto led = m_flow ? m_flow->currentLed() : std::nullopt;
    return led ? static_cast<int>(*led) : -1;
}

int KeyCalibration::passed() const
{
    return m_flow ? static_cast<int>(m_flow->passed()) : 0;
}

int KeyCalibration::total()
{
    return static_cast<int>(galleon::keyboard::LiveLedCount);
}

int KeyCalibration::skipped() const
{
    return m_flow ? static_cast<int>(m_flow->skippedLeds().size()) : 0;
}

bool KeyCalibration::finished() const
{
    return m_flow && m_flow->finished();
}

QString KeyCalibration::begin(bool startOver)
{
    if (m_flow)
        return tr("The calibration is already running");
    if (!m_lights.ready())
        return tr("Switch the live lighting on first: the calibration lights one LED at a time");
    galleon::keyboard::KeyMap known;
    if (!startOver) {
        if (const auto earlier = galleond::readKeyMap(m_path))
            known = earlier->map; // a damaged or missing file is simply not used
    }
    m_flow.emplace(known);
    settle(tr("Press the key that lit up. Nothing you press is kept: only which LED is which key."));
    return {};
}

void KeyCalibration::end()
{
    if (!m_flow)
        return;
    m_flow.reset();
    static_cast<void>(m_lights.showAll(RestRed, RestGreen, RestBlue));
    say(tr("The calibration is over. The answers are saved."));
}

void KeyCalibration::handleKey(unsigned nativeScanCode)
{
    if (!m_flow || m_flow->finished())
        return; // not asked for: every key is ignored
    const auto key = galleon::keyboard::hidUsageFromQtNativeScanCode(nativeScanCode, nativeSystem());
    if (!key) {
        say(tr("This key is not known to the calibration: try another key, or use \"Nothing lit up\"."));
        return;
    }
    applyKey(*key);
}

QVariantList KeyCalibration::specialKeys()
{
    // Print Screen, Pause, the Windows keys and Alt Gr are taken by the system on at least one of the two systems; Fn and Copilot send
    // no code of their own. The identities are those of KeyIdentity.h.
    struct Entry
    {
        QString label;
        galleon::keyboard::HidUsage usage;
    };
    const std::array<Entry, 9> entries{{{tr("Print Screen"), 0x46}, {tr("Pause"), 0x48}, {tr("Left Windows"), 0xE3}, {tr("Right Windows"), 0xE7},
                             {tr("Right Shift"), 0xE5}, {tr("Alt Gr"), 0xE6}, {tr("Right Control"), 0xE4},
                             {tr("Fn"), galleon::keyboard::FnKey}, {tr("Copilot"), galleon::keyboard::CopilotKey}}};
    QVariantList list;
    for (const Entry &entry : entries)
        list.append(QVariantMap{{QStringLiteral("label"), entry.label}, {QStringLiteral("usage"), static_cast<int>(entry.usage)}});
    return list;
}

void KeyCalibration::answerNamedKey(int usage)
{
    if (!m_flow || m_flow->finished())
        return;
    if (usage < 0 || usage > 0xFFFF) {
        say(tr("That answer was not accepted."));
        return;
    }
    applyKey(static_cast<galleon::keyboard::HidUsage>(usage));
}

void KeyCalibration::applyKey(galleon::keyboard::HidUsage key)
{
    if (!m_flow)
        return;
    const auto led = m_flow->currentLed();
    switch (m_flow->answerKey(key)) {
    case AnswerOutcome::Recorded: {
        // Shown until the next answer replaces it, so that a slip is seen at once; it is not kept.
        // The names are those of a QWERTY keyboard: for a key whose engraving depends on the layout, the message says so.
        const QString name = QString::fromLatin1(galleon::keyboard::keyName(key).value_or("?"));
        settle(galleon::keyboard::isLayoutDependent(key) ? tr("LED %1 = %2 (as named on a QWERTY keyboard).").arg(led.value_or(0)).arg(name)
                                                         : tr("LED %1 = %2.").arg(led.value_or(0)).arg(name));
        break;
    }
    case AnswerOutcome::KeyAlreadyUsed:
        say(tr("That key already belongs to LED %1. Press the key that is lit now.").arg(m_flow->conflictingLed().value_or(0)));
        break;
    case AnswerOutcome::Refused:
    case AnswerOutcome::NothingToUndo: say(tr("That answer was not accepted.")); break;
    }
}

void KeyCalibration::answerNone()
{
    if (!m_flow)
        return;
    if (m_flow->answerUnlit() == AnswerOutcome::Recorded)
        settle(tr("Recorded: nothing lit up."));
}

void KeyCalibration::answerStrip(int position)
{
    if (!m_flow)
        return;
    if (position < 1 || position > 255 || m_flow->answerStrip(static_cast<std::uint8_t>(position)) != AnswerOutcome::Recorded) {
        say(tr("A section of the light strip is counted from 1, starting at the left."));
        return;
    }
    settle(tr("Recorded: section %1 of the light strip.").arg(position));
}

void KeyCalibration::skip()
{
    if (!m_flow)
        return;
    m_flow->skip();
    settle(tr("Skipped: this LED has no answer yet."));
}

void KeyCalibration::back()
{
    if (!m_flow)
        return;
    if (m_flow->back() == AnswerOutcome::NothingToUndo) {
        say(tr("This is the first question."));
        return;
    }
    settle(tr("Back one question: answer it again."));
}

void KeyCalibration::settle(const QString &message)
{
    if (!m_flow)
        return;
    if (const auto led = m_flow->currentLed()) {
        const QString refused = m_lights.showLed(static_cast<int>(*led), LitRed, LitGreen, LitBlue);
        if (!refused.isEmpty()) {
            say(refused);
            return;
        }
        say(save() ? message : tr("The answers could not be saved: %1").arg(m_path));
    } else {
        static_cast<void>(m_lights.showAll(RestRed, RestGreen, RestBlue));
        say(save() ? tr("Every LED has been passed. The answers are saved.") : tr("The answers could not be saved: %1").arg(m_path));
    }
}

void KeyCalibration::say(const QString &message)
{
    m_message = message;
    Q_EMIT changed();
}

bool KeyCalibration::save()
{
    if (!m_flow)
        return false;
    galleond::KeyMapFile file;
    file.map = m_flow->map();
    file.source = QStringLiteral("Key calibration in Galleon Bosun: each LED of the live frame lit alone and the key pressed by the person, on their own keyboard.");
    return galleond::writeKeyMap(m_path, file);
}

} // namespace bosun

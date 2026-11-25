// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The Windows source of key presses, on a Windows machine: Windows is asked for the keyboard events only between start() and stop(), and what Windows gives that is not what
// is expected is dropped. A real key cannot be made to come here (the messages of Raw Input come from the system, from a device), so what is tested is what the system can
// be asked: what is registered with it, and what the window does with a message it should not have.
#include "WindowsRawKeyPressSource.h"

#include <QSignalSpy>
#include <QTest>

#include <windows.h>

#include <vector>

using namespace galleond;

namespace {

// What this process has registered with Windows for Raw Input (the keyboards: usage page 1, usage 6).
std::vector<RAWINPUTDEVICE> registeredDevices()
{
    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    std::vector<RAWINPUTDEVICE> devices(count);
    if (count > 0 && GetRegisteredRawInputDevices(devices.data(), &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1))
        devices.clear();
    return devices;
}

std::vector<RAWINPUTDEVICE> registeredKeyboards()
{
    std::vector<RAWINPUTDEVICE> keyboards;
    for (const RAWINPUTDEVICE &device : registeredDevices()) {
        if (device.usUsagePage == 0x01 && device.usUsage == 0x06)
            keyboards.push_back(device);
    }
    return keyboards;
}

} // namespace

class TestWindowsRawKeySource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(registeredKeyboards().empty()); // nothing of this process is registered before a test
    }

    void nothingIsRegisteredWithWindowsUntilItIsStarted()
    {
        const WindowsRawKeyPressSource source;
        QVERIFY(!source.watching());
        QVERIFY(source.windowForTests() == nullptr); // not even a window
        QVERIFY(registeredKeyboards().empty());
    }

    void startingRegistersTheKeyboardsForTheWindowInTheBackgroundWithoutBlockingTheKeys()
    {
        WindowsRawKeyPressSource source;
        const QSignalSpy gone(&source, &KeyPressSource::unavailable);
        source.start();
        QVERIFY(source.watching());
        QCOMPARE(gone.count(), 0);
        const auto keyboards = registeredKeyboards();
        QCOMPARE(keyboards.size(), std::size_t{1});
        QVERIFY((keyboards.front().dwFlags & RIDEV_INPUTSINK) != 0);  // the events come while another program has the focus
        QVERIFY((keyboards.front().dwFlags & RIDEV_NOLEGACY) == 0);   // and the keys still reach that program: nothing is blocked
        QVERIFY((keyboards.front().dwFlags & RIDEV_CAPTUREMOUSE) == 0);
        QVERIFY(keyboards.front().hwndTarget == static_cast<HWND>(source.windowForTests()));
        QVERIFY(source.windowForTests() != nullptr);
    }

    void stoppingUnregistersAtOnceAndTheWindowGoes()
    {
        WindowsRawKeyPressSource source;
        source.start();
        QVERIFY(!registeredKeyboards().empty());
        source.stop();
        QVERIFY(!source.watching());
        QVERIFY(registeredKeyboards().empty()); // from here Windows sends nothing to this process
        QVERIFY(source.windowForTests() == nullptr);
    }

    void startingTwiceRegistersOnceAndStoppingWithoutStartingIsHarmless()
    {
        WindowsRawKeyPressSource source;
        source.stop();
        source.start();
        source.start();
        QCOMPARE(registeredKeyboards().size(), std::size_t{1});
        source.stop();
        source.stop();
        QVERIFY(registeredKeyboards().empty());
    }

    void itCanBeStartedAgainAfterAStop()
    {
        WindowsRawKeyPressSource source;
        for (int round = 0; round < 3; ++round) {
            source.start();
            QVERIFY(source.watching());
            QCOMPARE(registeredKeyboards().size(), std::size_t{1});
            source.stop();
            QVERIFY(registeredKeyboards().empty());
        }
    }

    void destroyingTheSourceUnregisters()
    {
        {
            WindowsRawKeyPressSource source;
            source.start();
            QVERIFY(!registeredKeyboards().empty());
        }
        QVERIFY(registeredKeyboards().empty());
    }

    void aMessageOfRawInputThatWindowsDoesNotKnowIsDroppedWithoutAKeyAndWithoutACrash()
    {
        WindowsRawKeyPressSource source;
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        const auto window = static_cast<HWND>(source.windowForTests());
        QVERIFY(window != nullptr);
        // A WM_INPUT whose handle is not one that Windows gave: the data cannot be read, so there is nothing to say.
        SendMessageW(window, WM_INPUT, RIM_INPUT, 0);
        SendMessageW(window, WM_INPUT, RIM_INPUTSINK, static_cast<LPARAM>(0x1234));
        SendMessageW(window, WM_INPUT, RIM_INPUT, static_cast<LPARAM>(-1));
        QCoreApplication::processEvents();
        QCOMPARE(keys.count(), 0);
        QVERIFY(source.watching()); // and it goes on
    }

    void aMessageThatArrivesAfterTheStopIsNotRead()
    {
        WindowsRawKeyPressSource source;
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        source.stop();
        QVERIFY(source.windowForTests() == nullptr); // the window is gone: nothing can be sent to it
        QCOMPARE(keys.count(), 0);
    }
};

QTEST_MAIN(TestWindowsRawKeySource)
#include "test_windowsrawkeysource.moc"

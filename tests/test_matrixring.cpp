// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The green rain on the rings of the dials: a pure picture of the four segments of a ring, and the same palette as the keyboard's.
#include "LedPositions.h"
#include "MatrixRing.h"

#include <galleon/keyboard/MatrixRain.h>

#include <QTest>

#include <algorithm>
#include <array>

using namespace galleond;

namespace {

bool isDark(const RingRgb &colour)
{
    return colour == RingRgb{};
}

// The number of the frame at which the head of the ring of `dial` is on `segment` (the first one found in a cycle).
int frameWithHeadOn(int dial, int segment)
{
    for (int frame = 0; frame < MatrixRingCycle * 8; ++frame) {
        if (matrixRingSegments(dial, frame).at(static_cast<std::size_t>(segment)) == MatrixRingHead)
            return frame;
    }
    return -1;
}

} // namespace

class TestMatrixRing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theHeadIsTheAlmostWhiteOfTheRainAndGoesThroughEverySegmentOfTheRingInOrder()
    {
        for (int dial = 0; dial < galleon::protocol::DialCount; ++dial) {
            const int first = frameWithHeadOn(dial, 0);
            QVERIFY2(first >= 0, "the head never reaches the first segment");
            int previous = first;
            for (int segment = 1; segment < galleon::protocol::DialRingLeds; ++segment) {
                int frame = -1;
                for (int candidate = first; candidate < first + MatrixRingCycle * 4 && frame < 0; ++candidate) {
                    if (matrixRingSegments(dial, candidate).at(static_cast<std::size_t>(segment)) == MatrixRingHead)
                        frame = candidate;
                }
                QVERIFY2(frame > previous, "the head does not go from the first segment to the last");
                previous = frame;
            }
        }
    }

    void theFirstRingShowsItsHeadAtOnceWhenTheRainBegins()
    {
        QCOMPARE(matrixRingSegments(0, 0).at(0), MatrixRingHead);
    }

    void theTrailBehindTheHeadFadesFromGreenToBlack()
    {
        // With the head on the last segment, the three before it are the trail, from the brightest to the dimmest.
        const int frame = frameWithHeadOn(0, 3);
        QVERIFY(frame >= 0);
        const auto colours = matrixRingSegments(0, frame);
        QCOMPARE(colours.at(3), MatrixRingHead);
        QCOMPARE(colours.at(2), (RingRgb{0, 159, 44}));  // three quarters of #00d43a
        QCOMPARE(colours.at(1), (RingRgb{0, 106, 29}));  // a half
        QCOMPARE(colours.at(0), (RingRgb{0, 53, 15}));   // a quarter
        for (int segment = 0; segment < 3; ++segment) {
            QCOMPARE(int(colours.at(static_cast<std::size_t>(segment)).red), 0); // green only
            QVERIFY(colours.at(static_cast<std::size_t>(segment)).green > 0);
        }
    }

    void whatTheHeadHasNotReachedIsDark()
    {
        // With the head on the first segment, the other three have not been reached.
        const int frame = frameWithHeadOn(1, 0);
        QVERIFY(frame >= 0);
        const auto colours = matrixRingSegments(1, frame);
        QCOMPARE(colours.at(0), MatrixRingHead);
        for (int segment = 1; segment < galleon::protocol::DialRingLeds; ++segment)
            QVERIFY(isDark(colours.at(static_cast<std::size_t>(segment))));
    }

    void theRingRestsDarkBetweenTwoPassesAndTheRainBeginsAgain()
    {
        bool sawDark = false;
        for (int frame = 0; frame < MatrixRingCycle * 8; ++frame) {
            const auto colours = matrixRingSegments(0, frame);
            sawDark = sawDark || std::all_of(colours.begin(), colours.end(), isDark);
        }
        QVERIFY(sawDark); // the loop has a moment of rest
        // A loop: one cycle later (in frames) the same picture.
        constexpr int FramesPerCycleOfRing0 = MatrixRingCycle * 4; // the first ring moves one step every four pictures
        for (int frame = 0; frame < 20; ++frame)
            QCOMPARE(matrixRingSegments(0, frame), matrixRingSegments(0, frame + FramesPerCycleOfRing0));
    }

    void theTwoRingsDoNotMoveTogether()
    {
        bool differs = false;
        for (int frame = 0; frame < MatrixRingCycle * 8 && !differs; ++frame)
            differs = matrixRingSegments(0, frame) != matrixRingSegments(1, frame);
        QVERIFY(differs);
    }

    void aRingStaysThePictureOfItsFrameBetweenTwoStepsOfTheHead()
    {
        // A step lasts several pictures of the rain: nothing changes within it (no needless report to the module).
        QCOMPARE(matrixRingSegments(0, 0), matrixRingSegments(0, 3));
        QVERIFY(matrixRingSegments(0, 3) != matrixRingSegments(0, 4));
    }

    void aDialOrAFrameOutOfRangeIsDark()
    {
        for (const auto &colours : {matrixRingSegments(-1, 0), matrixRingSegments(2, 0), matrixRingSegments(0, -1)})
            QVERIFY(std::all_of(colours.begin(), colours.end(), isDark));
    }

    void theColoursAreTheOnesOfTheKeyboardRain()
    {
        // The module has its own copy of the palette (the two parts of galleond do not call each other): it must stay the keyboard's.
        const auto &positions = builtInLedPositions();
        bool headSeen = false;
        for (double seconds = 0.0; seconds < 60.0 && !headSeen; seconds += galleon::keyboard::MatrixTickSeconds) {
            for (const galleon::keyboard::Rgb &colour : galleon::keyboard::matrixRainColours(positions, seconds)) {
                if (colour.red > 200) {
                    QCOMPARE(int(colour.red), int(MatrixRingHead.red));
                    QCOMPARE(int(colour.green), int(MatrixRingHead.green));
                    QCOMPARE(int(colour.blue), int(MatrixRingHead.blue));
                    headSeen = true;
                    break;
                }
            }
        }
        QVERIFY(headSeen);
        // And the trail has the hue of the keyboard's: the same green, only dimmer.
        const double ratio = static_cast<double>(MatrixRingTrail.green) / MatrixRingTrail.blue;
        for (int frame = 0; frame < MatrixRingCycle * 8; ++frame) {
            for (const RingRgb &colour : matrixRingSegments(0, frame)) {
                if (colour == MatrixRingHead || isDark(colour))
                    continue;
                QCOMPARE(int(colour.red), 0);
                QVERIFY(std::abs(static_cast<double>(colour.green) / colour.blue - ratio) < 0.5);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestMatrixRing)
#include "test_matrixring.moc"

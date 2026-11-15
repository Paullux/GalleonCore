// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveScene.h"

#include "galleon/keyboard/LiveFrame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace galleon::keyboard {

namespace {

[[nodiscard]] bool channelInRange(int value)
{
    return value >= 0 && value <= 255;
}

} // namespace

std::optional<LiveScene> uniformScene(int red, int green, int blue)
{
    if (!channelInRange(red) || !channelInRange(green) || !channelInRange(blue))
        return std::nullopt;
    LiveScene scene;
    scene.kind = LiveScene::Kind::Uniform;
    scene.red = static_cast<std::uint8_t>(red);
    scene.green = static_cast<std::uint8_t>(green);
    scene.blue = static_cast<std::uint8_t>(blue);
    return scene;
}

std::optional<LiveScene> singleLedScene(int led, int red, int green, int blue)
{
    if (led < 0 || led >= static_cast<int>(LiveLedCount))
        return std::nullopt;
    auto scene = uniformScene(red, green, blue);
    if (!scene)
        return std::nullopt;
    scene->kind = LiveScene::Kind::SingleLed;
    scene->led = static_cast<std::uint16_t>(led);
    return scene;
}

std::optional<LiveFrame> sceneFrame(const LiveScene &scene)
{
    std::vector<LedColor> leds;
    if (scene.kind == LiveScene::Kind::SingleLed) {
        leds.push_back({scene.led, scene.red, scene.green, scene.blue});
    } else {
        leds.reserve(LiveLedCount);
        for (std::size_t led = 0; led < LiveLedCount; ++led)
            leds.push_back({static_cast<std::uint16_t>(led), scene.red, scene.green, scene.blue});
    }
    return buildLiveFrame(leds); // refuses a LED beyond the frame, which only a scene made by hand can hold
}

} // namespace galleon::keyboard

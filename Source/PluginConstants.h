#pragma once

#include <cstdint>

namespace MaxOxConfig
{
inline constexpr auto kDonateUrl = "https://destream.net/live/DjSher/donate";

inline constexpr std::int64_t kInitialSupportPromptDelayMs =
    14LL * 24 * 60 * 60 * 1000;
inline constexpr std::int64_t kMinimumSupportPromptDelayMs = 1 * 60 * 1000;
inline constexpr std::int64_t kSupportPromptCloseDivisor = 2;
}

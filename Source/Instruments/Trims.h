#pragma once
// Instrument mode: each instrument's level trim (dB), so that stepping through the
// browser keeps a similar loudness (the instruments were balanced as parts of their
// Places, where a shaker sits far under a bowed bass). Measured, not by ear: the loudest
// 300 ms of one held note at velocity 100 (A3; A2 for basses; C3 for kits), dry, brought
// toward -28 dB, at most 12 dB either way. Regenerate with
//   WorldbuilderTests --only insttrims
// and paste its table here. Unlisted: 0. (The woodwinds pass re-measured only its four woodwind
// parts: the Village's recorder, the Forest's flute, the Desert's ney, the Mining beacon.)
#include <array>

namespace wb::inst {

// clang-format off
inline constexpr std::array<std::array<float, 6>, 27> levelTrimDb { {
    { 0, 0, 0, 0, 0, 0 },
    { -11.0f, -8.0f, -2.0f, -7.5f, -2.5f, 1.0f },   // Castle
    { 6.0f, 11.5f, -2.0f, -9.0f, 12.0f, 0.0f },   // Village
    { 3.5f, 6.5f, -7.0f, -5.5f, 4.5f, 5.5f },   // Spaceport
    { -5.0f, 0.0f, 1.0f, 4.0f, 8.5f, -4.5f },   // Factory
    { -11.5f, -5.0f, 1.0f, 8.0f, -3.5f, 10.5f },   // City
    { -2.0f, -1.0f, 1.0f, 0.0f, 4.5f, 12.0f },   // Forest
    { 1.5f, 0.0f, 12.0f, -7.5f, 12.0f, 12.0f },   // Alien Hive
    { 5.5f, -5.5f, -8.0f, 3.0f, -4.5f, 2.5f },   // Temple
    { -2.5f, 8.0f, -5.5f, -10.0f, -4.5f, -7.0f },   // Cavern
    { -1.5f, -3.5f, -3.5f, 6.5f, 12.0f, -1.0f },   // Ruins
    { 3.0f, 8.5f, 0.0f, 0.0f, 3.5f, 6.0f },   // Spaceship
    { -3.5f, 4.0f, -0.5f, -4.5f, 5.0f, 12.0f },   // Biodome
    { 12.0f, -1.5f, 9.0f, -9.0f, 8.0f, 0.0f },   // Sea Ship
    { 6.5f, 5.5f, 5.5f, 12.0f, 7.5f, -4.5f },   // Suburbs
    { -1.0f, -0.5f, -7.0f, 5.0f, 11.5f, -2.5f },   // Mall
    { 3.5f, -5.0f, -0.5f, 10.0f, 5.0f, -2.5f },   // Military Camp
    { 8.0f, -2.5f, 4.0f, 12.0f, 9.0f, 2.0f },   // Beach
    { -2.5f, 0.5f, 11.0f, 4.5f, 12.0f, 0.5f },   // Desert
    { 8.5f, 10.5f, -4.5f, 0.5f, 3.5f, 7.0f },   // Palace
    { 5.0f, 1.5f, 4.0f, 3.0f, -3.0f, 2.0f },   // Tavern
    { -3.5f, -3.5f, 0.5f, 1.0f, -0.5f, -3.5f },   // Space Station
    { 0.0f, -0.5f, -3.0f, -5.0f, 3.0f, 6.0f },   // Cyberpunk Megacity
    { -0.5f, 8.5f, 3.0f, -1.5f, 2.0f, -8.0f },   // Starship Bridge
    { 3.0f, -5.5f, 1.0f, 8.0f, 9.5f, -4.5f },   // Asteroid Mining Colony
    { -4.0f, 1.5f, 10.5f, 5.0f, -0.5f, 6.0f },   // Alien Ruins
    { -1.0f, 12.0f, -2.5f, 3.0f, 0.0f, -2.5f },   // Nebula
} };
// clang-format on
// The Drama instruments (the Drama tier rework: drums, ostinato, hits), per Place, measured
// the same way (WorldbuilderTests --only dramatrims prints this table).
// clang-format off
inline constexpr std::array<std::array<float, 3>, 27> dramaTrimDb { {
    { 0, 0, 0 },
    { -8.0f, -9.5f, 3.5f },   // Castle
    { -2.5f, -1.0f, 3.5f },   // Village
    { 0.5f, 0.0f, -4.0f },   // Spaceport
    { 0.0f, -4.0f, -10.0f },   // Factory
    { 1.0f, -4.0f, -6.0f },   // City
    { -3.0f, -9.5f, -4.5f },   // Forest
    { 0.5f, 0.0f, -4.0f },   // Alien Hive
    { -8.5f, -9.5f, -4.0f },   // Temple
    { -9.5f, -4.0f, -8.5f },   // Cavern
    { -8.5f, -9.5f, -8.5f },   // Ruins
    { 0.5f, 0.0f, -4.0f },   // Spaceship
    { -2.5f, 0.0f, -6.0f },   // Biodome
    { -8.0f, -9.5f, 3.5f },   // Sea Ship
    { -4.5f, -6.5f, -8.5f },   // Suburbs
    { 1.0f, 0.0f, -6.0f },   // Mall
    { -4.5f, -9.5f, 3.5f },   // Military Camp
    { -2.5f, -1.0f, 3.5f },   // Beach
    { -2.5f, -1.0f, 3.5f },   // Desert
    { -8.0f, -9.5f, 3.5f },   // Palace
    { -2.0f, -1.0f, 3.5f },   // Tavern
    { 0.5f, 0.0f, -4.0f },   // Space Station
    { 0.5f, -4.0f, -6.0f },   // Cyberpunk Megacity
    { 0.5f, 0.0f, 3.5f },   // Starship Bridge
    { -0.5f, -4.0f, -10.0f },   // Asteroid Mining Colony
    { -9.5f, 0.0f, -4.0f },   // Alien Ruins
    { 0.5f, 0.0f, -4.0f },   // Nebula
} };
// clang-format on
inline constexpr float levelTrim(int type, int element) {
    const auto t = std::size_t(type < 0 || type > 26 ? 0 : type);
    if (element >= 6 && element <= 8) return dramaTrimDb[t][std::size_t(element - 6)];
    return levelTrimDb[t][std::size_t(element < 0 || element > 5 ? 0 : element)];
}

} // namespace wb::inst

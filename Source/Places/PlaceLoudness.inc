// The level and drums pass: each Place's loudness at its card's defaults (LUFS, integrated,
// Score Intensity 50 %, Drama 0; measured, the same as before the Drama tier: Tests/LevelTable.inc).
// The orchestration make-up weighs each Place's instruments by it.
inline constexpr std::array<float, PlaceTypeCount> placeLoudnessTable { {
    -30.0f,   // none
    -22.36f,   // castle
    -28.20f,   // village
    -25.65f,   // spaceport
    -25.33f,   // factory
    -26.73f,   // city
    -25.01f,   // forest
    -24.06f,   // hive
    -24.90f,   // temple
    -27.71f,   // cavern
    -27.05f,   // ruins
    -27.44f,   // spaceship
    -25.37f,   // biodome
    -28.19f,   // seaship
    -26.51f,   // suburbs
    -26.27f,   // mall
    -27.19f,   // military
    -27.99f,   // beach
    -29.54f,   // desert
    -30.50f,   // palace
    -28.38f,   // tavern
    -24.58f,   // station
    -26.63f,   // megacity
    -28.10f,   // bridge
    -28.13f,   // mining
    -28.56f,   // alienruins
    -26.83f,   // nebula
} };

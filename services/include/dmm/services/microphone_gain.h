// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

namespace dmm::services::microphone_gain {

/// The microphone gain for transmit, in whole dB from MinDb to MaxDb.
/// StandardDb is the level the board is set up for; each dB up or down
/// changes the transmitted voice by 1 dB. The board decides which stage
/// takes it on each kind of channel.
inline constexpr int MinDb = 0;
inline constexpr int MaxDb = 30;
inline constexpr int StandardDb = 16;

/// Stored formats that keep the gain as a level 1..Levels: the middle
/// level is StandardDb, each level LevelStepDb more than the one below.
inline constexpr int Levels = 5;
inline constexpr int LevelStepDb = 4;

/// The gain for level @p level (1..Levels); a level out of range is the
/// middle one.
constexpr int fromLevel(int level)
{
    if (level < 1 || level > Levels) {
        level = (Levels + 1) / 2;
    }
    return StandardDb + (level - (Levels + 1) / 2) * LevelStepDb;
}

/// @p db limited to MinDb..MaxDb.
constexpr int clamp(int db)
{
    return db < MinDb ? MinDb : db > MaxDb ? MaxDb
                                           : db;
}

} // namespace dmm::services::microphone_gain

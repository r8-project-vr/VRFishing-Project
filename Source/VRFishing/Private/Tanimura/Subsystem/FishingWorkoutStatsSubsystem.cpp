// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Subsystem/FishingWorkoutStatsSubsystem.h"

void UFishingWorkoutStatsSubsystem::AddExerciseSeconds(float Seconds)
{
    // 0以下の値は集計を乱すため加算しない
    if (Seconds <= 0.0f) {
        return;
    }

    TotalExerciseSeconds += Seconds;
}

void UFishingWorkoutStatsSubsystem::AddArmUpDownCount(int32 Count)
{
    // 0以下の値は集計を乱すため加算しない
    if (Count <= 0) {
        return;
    }

    TotalArmUpDownCount += Count;
}

void UFishingWorkoutStatsSubsystem::AddReelRevolutionCount(int32 Count)
{
    // 0以下の値は集計を乱すため加算しない
    if (Count <= 0) {
        return;
    }

    TotalReelRevolutionCount += Count;
}

void UFishingWorkoutStatsSubsystem::AddEscapedFish()
{
    // セット失敗1回を逃がした魚1匹として数える
    EscapedFishCount++;
}

void UFishingWorkoutStatsSubsystem::ResetWorkoutStats()
{
    // 前回プレイの成績を破棄する
    TotalExerciseSeconds = 0.0f;
    TotalArmUpDownCount = 0;
    TotalReelRevolutionCount = 0;
    EscapedFishCount = 0;
}

float UFishingWorkoutStatsSubsystem::GetTotalExerciseSeconds() const
{
    return TotalExerciseSeconds;
}

FText UFishingWorkoutStatsSubsystem::GetTotalExerciseTimeText() const
{
    // 秒を切り捨てて分と秒に分解する
    const int32 TotalSeconds = FMath::FloorToInt(TotalExerciseSeconds);
    const int32 Minutes = TotalSeconds / 60;
    const int32 Seconds = TotalSeconds % 60;

    // リザルト表示用に「〇分〇秒」形式へ整形する
    return FText::FromString(FString::Printf(TEXT("%d分%d秒"), Minutes, Seconds));
}

int32 UFishingWorkoutStatsSubsystem::GetTotalArmUpDownCount() const
{
    return TotalArmUpDownCount;
}

int32 UFishingWorkoutStatsSubsystem::GetTotalReelRevolutionCount() const
{
    return TotalReelRevolutionCount;
}

int32 UFishingWorkoutStatsSubsystem::GetEscapedFishCount() const
{
    return EscapedFishCount;
}

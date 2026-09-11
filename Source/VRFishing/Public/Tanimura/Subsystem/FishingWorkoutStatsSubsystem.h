// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FishingWorkoutStatsSubsystem.generated.h"

/**
 * 1ゲーム分の運動成績（運動時間・腕上下回数・リール回転回数・逃がした魚数）を保持するサブシステム
 * GameInstanceに属するため、LV_MainGameからLV_GameResultへ遷移しても成績が残る
 */
UCLASS()
class VRFISHING_API UFishingWorkoutStatsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 運動していた秒数を加算する（GameModeのTickから呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddExerciseSeconds(float Seconds);

    // 今回セットの腕上下回数を加算する（VRPawnの完了通知から呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddArmUpDownCount(int32 Count);

    // 今回セットのリール回転回数を加算する（VRPawnの完了通知から呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddReelRevolutionCount(int32 Count);

    // 逃がした魚を1匹加算する（セット失敗時に呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddEscapedFish();

    // すべての成績を破棄する（ゲーム開始時に呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void ResetWorkoutStats();

    // 合計運動時間（秒）を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Workout")
    float GetTotalExerciseSeconds() const;

    // 合計運動時間を表示用テキスト（例：2分30秒）で返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Workout")
    FText GetTotalExerciseTimeText() const;

    // 合計腕上下回数を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Workout")
    int32 GetTotalArmUpDownCount() const;

    // 合計リール回転回数を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Workout")
    int32 GetTotalReelRevolutionCount() const;

    // 逃がした魚の数を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Workout")
    int32 GetEscapedFishCount() const;

private:
    // 合計運動時間（秒）
    UPROPERTY()
    float TotalExerciseSeconds = 0.0f;

    // 合計腕上下回数
    UPROPERTY()
    int32 TotalArmUpDownCount = 0;

    // 合計リール回転回数
    UPROPERTY()
    int32 TotalReelRevolutionCount = 0;

    // 逃がした魚の数
    UPROPERTY()
    int32 EscapedFishCount = 0;
};

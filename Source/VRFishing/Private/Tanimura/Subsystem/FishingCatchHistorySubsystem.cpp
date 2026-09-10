// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Subsystem/FishingCatchHistorySubsystem.h"

#include "Engine/Texture2D.h"

void UFishingCatchHistorySubsystem::SetFishDisplays(const TArray<FFishDisplayInfo>& InFishDisplays)
{
    // GameModeのClass Defaultsで設定された魚表示情報を受け取る
    FishDisplays = InFishDisplays;
}

void UFishingCatchHistorySubsystem::SetCaughtFishLevel(int32 FishLevel)
{
    // 今回釣った魚のレベルを記録する（釣れなかったときは0）
    CaughtFishLevel = FishLevel;
}

void UFishingCatchHistorySubsystem::AddCaughtFish(int32 FishLevel)
{
    // 釣った順にレベルを積む（重複もそのまま残す）
    CaughtFishLevels.Add(FishLevel);
}

void UFishingCatchHistorySubsystem::ResetCaughtFish()
{
    // 釣果履歴と今回のセット結果を破棄する
    CaughtFishLevels.Reset();
    CaughtFishLevel = 0;
}

int32 UFishingCatchHistorySubsystem::GetCaughtFishLevel() const
{
    return CaughtFishLevel;
}

TArray<int32> UFishingCatchHistorySubsystem::GetCaughtFishLevels() const
{
    // BP側で書き換えられても内部状態を壊さないようコピーを返す
    return CaughtFishLevels;
}

int32 UFishingCatchHistorySubsystem::GetCaughtFishCount() const
{
    return CaughtFishLevels.Num();
}

FText UFishingCatchHistorySubsystem::GetFishNameByLevel(int32 Level) const
{
    // 範囲外は空テキストを返す（毎フレーム呼ばれてもログを出さない）
    if (!IsValidLevel(Level)) {
        return FText::GetEmpty();
    }
    return FishDisplays[LevelToIndex(Level)].FishName;
}

UTexture2D* UFishingCatchHistorySubsystem::GetFishTextureByLevel(int32 Level) const
{
    // 範囲外はnullptrを返す（毎フレーム呼ばれてもログを出さない）
    if (!IsValidLevel(Level)) {
        return nullptr;
    }
    return FishDisplays[LevelToIndex(Level)].FishTexture;
}

int32 UFishingCatchHistorySubsystem::GetFishDisplayCount() const
{
    return FishDisplays.Num();
}

bool UFishingCatchHistorySubsystem::IsValidLevel(int32 Level) const
{
    // レベルは1始まりのため0以下は無効として弾く
    if (Level < 1) {
        return false;
    }

    // 設定済みの魚表示情報の範囲内か判定する
    if (Level > FishDisplays.Num()) {
        return false;
    }

    return true;
}

int32 UFishingCatchHistorySubsystem::LevelToIndex(int32 Level) const
{
    // レベル1がインデックス0に対応する
    return Level - 1;
}

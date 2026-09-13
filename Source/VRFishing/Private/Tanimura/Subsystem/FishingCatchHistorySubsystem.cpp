// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Subsystem/FishingCatchHistorySubsystem.h"

#include "Engine/Texture2D.h"

void UFishingCatchHistorySubsystem::SetFishDisplays(const TArray<FFishDisplayInfo>& InFishDisplays)
{
    // GameModeのClass Defaultsで設定された通常魚の表示情報を受け取る
    FishDisplays = InFishDisplays;
}

void UFishingCatchHistorySubsystem::SetRareFishDisplays(const TArray<FFishDisplayInfo>& InRareFishDisplays)
{
    // GameModeのClass Defaultsで設定されたレア魚の表示情報を受け取る
    RareFishDisplays = InRareFishDisplays;
}

void UFishingCatchHistorySubsystem::SetCaughtFishLevel(int32 FishLevel, bool bIsRare)
{
    // 今回釣った魚の釣果を記録する（釣れなかったときはレベル0）
    CurrentSetFishRecord = BuildCatchRecord(FishLevel, bIsRare);
}

void UFishingCatchHistorySubsystem::AddCaughtFish(int32 FishLevel, bool bIsRare)
{
    // 釣った順に釣果を積む（重複もそのまま残す）
    CaughtFishRecords.Add(BuildCatchRecord(FishLevel, bIsRare));
}

void UFishingCatchHistorySubsystem::ResetCaughtFish()
{
    // 釣果履歴と今回のセット結果を破棄する
    CaughtFishRecords.Reset();
    CurrentSetFishRecord = FFishCatchRecord();
}

FFishCatchRecord UFishingCatchHistorySubsystem::GetCurrentSetFishRecord() const
{
    return CurrentSetFishRecord;
}

FFishCatchRecord UFishingCatchHistorySubsystem::GetCaughtFishRecord(int32 Index) const
{
    // 範囲外は空の釣果を返す（毎フレーム呼ばれてもログを出さない）
    if (!CaughtFishRecords.IsValidIndex(Index)) {
        return FFishCatchRecord();
    }
    return CaughtFishRecords[Index];
}

int32 UFishingCatchHistorySubsystem::GetCaughtFishLevel() const
{
    return CurrentSetFishRecord.FishLevel;
}

TArray<int32> UFishingCatchHistorySubsystem::GetCaughtFishLevels() const
{
    // 旧APIの互換用に、釣果履歴からレベルだけを取り出す
    TArray<int32> Levels;
    Levels.Reserve(CaughtFishRecords.Num());
    for (const FFishCatchRecord& Record : CaughtFishRecords) {
        Levels.Add(Record.FishLevel);
    }
    return Levels;
}

int32 UFishingCatchHistorySubsystem::GetCaughtFishCount() const
{
    return CaughtFishRecords.Num();
}

FText UFishingCatchHistorySubsystem::GetFishNameByLevelAndRare(int32 Level, bool bIsRare) const
{
    // レア指定かつレア魚が設定済みのときだけレア魚を返す（未設定なら通常魚へフォールバック）
    if (bIsRare && IsValidRareLevel(Level)) {
        return RareFishDisplays[LevelToIndex(Level)].FishName;
    }

    // 範囲外は空テキストを返す（毎フレーム呼ばれてもログを出さない）
    if (!IsValidLevel(Level)) {
        return FText::GetEmpty();
    }
    return FishDisplays[LevelToIndex(Level)].FishName;
}

UTexture2D* UFishingCatchHistorySubsystem::GetFishTextureByLevelAndRare(int32 Level, bool bIsRare) const
{
    // レア指定かつレア魚が設定済みのときだけレア魚を返す（未設定なら通常魚へフォールバック）
    if (bIsRare && IsValidRareLevel(Level)) {
        return RareFishDisplays[LevelToIndex(Level)].FishTexture;
    }

    // 範囲外はnullptrを返す（毎フレーム呼ばれてもログを出さない）
    if (!IsValidLevel(Level)) {
        return nullptr;
    }
    return FishDisplays[LevelToIndex(Level)].FishTexture;
}

FText UFishingCatchHistorySubsystem::GetFishNameByLevel(int32 Level) const
{
    // 通常魚のみを返す
    return GetFishNameByLevelAndRare(Level, false);
}

UTexture2D* UFishingCatchHistorySubsystem::GetFishTextureByLevel(int32 Level) const
{
    // 通常魚のみを返す
    return GetFishTextureByLevelAndRare(Level, false);
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

    // 設定済みの通常魚表示情報の範囲内か判定する
    if (Level > FishDisplays.Num()) {
        return false;
    }

    return true;
}

bool UFishingCatchHistorySubsystem::IsValidRareLevel(int32 Level) const
{
    // レベルは1始まりのため0以下は無効として弾く
    if (Level < 1) {
        return false;
    }

    // 設定済みのレア魚表示情報の範囲内か判定する
    if (Level > RareFishDisplays.Num()) {
        return false;
    }

    return true;
}

int32 UFishingCatchHistorySubsystem::LevelToIndex(int32 Level) const
{
    // レベル1がインデックス0に対応する
    return Level - 1;
}

FFishCatchRecord UFishingCatchHistorySubsystem::BuildCatchRecord(int32 Level, bool bIsRare) const
{
    // 表示情報を引いて1匹分の釣果にまとめる
    FFishCatchRecord Record;
    Record.FishLevel = Level;
    Record.bIsRare = bIsRare;
    Record.FishName = GetFishNameByLevelAndRare(Level, bIsRare);
    Record.FishTexture = GetFishTextureByLevelAndRare(Level, bIsRare);
    return Record;
}

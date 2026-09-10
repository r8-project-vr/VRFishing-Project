// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FishingCatchHistorySubsystem.generated.h"

class UTexture2D;

/**
 * レベル別の魚表示情報（リザルトUIへ出す魚名とイラスト）
 * 配列のインデックス0がレベル1に対応する
 */
USTRUCT(BlueprintType)
struct FFishDisplayInfo
{
    GENERATED_BODY()

    // リザルトUIへ表示する魚の名前（例：クマノミ）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Catch")
    FText FishName;

    // リザルトUIへ表示する魚のイラスト（インポートしたテクスチャを割り当てる）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Catch")
    TObjectPtr<UTexture2D> FishTexture = nullptr;
};

/**
 * 釣果（釣った魚のレベル）とレベル別の魚表示情報を保持するサブシステム
 * GameInstanceに属するため、LV_MainGameからLV_GameResultへ遷移しても釣果が残る
 */
UCLASS()
class VRFISHING_API UFishingCatchHistorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // レベル別の魚表示情報を設定する（GameModeのBeginPlayで呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Catch")
    void SetFishDisplays(const TArray<FFishDisplayInfo>& InFishDisplays);

    // 今回釣った魚のレベルを設定する（釣れなかったときは0を渡す）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Catch")
    void SetCaughtFishLevel(int32 FishLevel);

    // 釣った魚のレベルを履歴へ追加する（釣った順に積む。重複もそのまま残す）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Catch")
    void AddCaughtFish(int32 FishLevel);

    // 釣果履歴をすべて破棄する（ゲーム開始時に呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Catch")
    void ResetCaughtFish();

    // 今回のセットで釣った魚のレベルを返す（0=釣れていない）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    int32 GetCaughtFishLevel() const;

    // これまでに釣った魚のレベル一覧を返す（釣った順・重複あり）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    TArray<int32> GetCaughtFishLevels() const;

    // これまでに釣った魚の総数を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    int32 GetCaughtFishCount() const;

    // 指定レベルに対応する魚の表示名を返す（未設定時は空テキスト）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    FText GetFishNameByLevel(int32 Level) const;

    // 指定レベルに対応する魚のイラストを返す（未設定時はnullptr）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    UTexture2D* GetFishTextureByLevel(int32 Level) const;

    // 設定されている魚表示情報の件数を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    int32 GetFishDisplayCount() const;

private:
    // レベルが表示情報の範囲内か判定する（レベルは1始まり）
    bool IsValidLevel(int32 Level) const;

    // レベルを配列インデックスへ変換する（レベル1がインデックス0）
    int32 LevelToIndex(int32 Level) const;

    // レベル別の魚表示情報（インデックス0がレベル1）
    UPROPERTY()
    TArray<FFishDisplayInfo> FishDisplays;

    // 釣った魚のレベル履歴（釣った順・重複あり）
    UPROPERTY()
    TArray<int32> CaughtFishLevels;

    // 今回のセットで釣った魚のレベル（0=釣れていない）
    UPROPERTY()
    int32 CaughtFishLevel = 0;
};

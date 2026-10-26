// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FishingCatchSaveSubsystem.generated.h"

class UFishDataAsset;

/**
 * 水族館へ引き継ぐ1匹分の釣果
 */
USTRUCT(BlueprintType)
struct FFishCatchSaveEntry
{
	GENERATED_BODY()

	// 水族館で表示する魚種データ
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Fishing|Save")
	TSoftObjectPtr<UFishDataAsset> FishData;

	// 釣ったときの運動レベル
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Fishing|Save")
	int32 FishLevel = 0;

	// レア魚かどうか
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Fishing|Save")
	bool bIsRare = false;
};

/**
 * 釣果をローカルファイルへ保存するセーブデータ
 */
UCLASS()
class VRFISHING_API UFishingCatchSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 釣った順の魚一覧
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Fishing|Save")
	TArray<FFishCatchSaveEntry> CaughtFish;
};

/**
 * メインゲームの釣果をローカルファイルへ保存し、水族館へ引き継ぐサブシステム
 */
UCLASS()
class VRFISHING_API UFishingCatchSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 保存データを読み込む
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 1匹分の釣果を履歴へ追加して即時保存
	UFUNCTION(BlueprintCallable, Category = "Fishing|Save")
	void AddCaughtFish(UFishDataAsset* FishData, int32 FishLevel, bool bIsRare);

	// 保存済みの釣果一覧を釣った順で返す
	UFUNCTION(BlueprintPure, Category = "Fishing|Save")
	TArray<FFishCatchSaveEntry> GetCaughtFish() const;

	// 保存済みの釣果数を返す
	UFUNCTION(BlueprintPure, Category = "Fishing|Save")
	int32 GetCaughtFishCount() const;

	// 釣果をすべて破棄して保存
	UFUNCTION(BlueprintCallable, Category = "Fishing|Save")
	void ClearCaughtFish();

	// 現在の内容をローカルファイルへ保存
	UFUNCTION(BlueprintCallable, Category = "Fishing|Save")
	bool SaveToDisk();

private:
	// ローカルファイルから読み込む（失敗時は空データを用意）
	void LoadFromDisk();

	// セーブデータ
	UPROPERTY()
	TObjectPtr<UFishingCatchSaveGame> SaveData;

	// セーブスロット名
	static const FString SaveSlotName;
};

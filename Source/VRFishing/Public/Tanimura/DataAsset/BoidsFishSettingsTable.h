// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoidsFishSettingsTable.generated.h"

class UFishDataAsset;
class UBoidsFishSettings;

/**
 * 魚種と操舵パラメータ・生成上限の対応1件分
 */
USTRUCT(BlueprintType)
struct FBoidsFishSettingsTableEntry
{
	GENERATED_BODY()

	// 対象の魚種データ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids")
	TSoftObjectPtr<UFishDataAsset> FishData;

	// この魚種で使う操舵パラメータ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids")
	TSoftObjectPtr<UBoidsFishSettings> Settings;

	// 水族館で同時に泳がせる上限数。0なら上限なし
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids", meta = (ClampMin = "0"))
	int32 MaxCount = 0;
};

/**
 * 魚種ごとのBoids設定と生成上限をまとめた対応表
 */
UCLASS(BlueprintType)
class VRFISHING_API UBoidsFishSettingsTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 魚種と設定の対応。上から順に検索する
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids")
	TArray<FBoidsFishSettingsTableEntry> Entries;

	// 指定した魚種の設定を返す。未登録ならnullptr
	UFUNCTION(BlueprintPure, Category = "Boids")
	UBoidsFishSettings* FindSettings(const UFishDataAsset* FishData) const;

	// 指定した魚種の上限数を返す。未登録または0なら上限なし
	UFUNCTION(BlueprintPure, Category = "Boids")
	int32 FindMaxCount(const UFishDataAsset* FishData) const;

private:
	// 指定した魚種の項目を返す
	const FBoidsFishSettingsTableEntry* FindEntry(const UFishDataAsset* FishData) const;
};

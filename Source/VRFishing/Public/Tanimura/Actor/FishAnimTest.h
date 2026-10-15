// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FishAnimTest.generated.h"

class USceneComponent;

/**
 * 魚の遊泳アニメーションをテストするアクター
 * ボーン、メッシュの配置はBP側で行う
 */
UCLASS(Abstract)
class VRFISHING_API AFishAnimTest : public AActor
{
	GENERATED_BODY()

public:
	AFishAnimTest();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	//ルートコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FishAnimTest|Components")
	USceneComponent* RootScene;

	//尾の最大振幅（親ボーンを基準とした最大角度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Wave", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxAmplitude = 20.0f;

	//周波数（大きいほど速く振れる）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Wave", meta = (ClampMin = "0.0"))
	float Frequency = 1.5f;

	//体に乗る波の数（大きいほど波が細かくなる）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Wave", meta = (ClampMin = "0.0"))
	float WaveCount = 0.8f;

	//振幅の増大度（大きいほど頭が硬く、尾だけが振れる）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Wave", meta = (ClampMin = "0.1"))
	float AmplitudeExponent = 2.0f;

	//泳ぐ速さ（速いほど速く振れる）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Swim", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SwimSpeed = 150.0f;

	//速さ1あたりの周波数増加量（大きいほど速さの影響が大きくなる）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishAnimTest|Swim", meta = (ClampMin = "0.0"))
	float FrequencyPerSpeed = 0.0f;

private:
	//全ボーンを格納した配列（頭→尾）
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "FishAnimTest|Debug")
	TArray<USceneComponent*> SpineBones;

	//各Boneの初期回転
	TArray<FRotator> InitialRotations;

	//経過時間（波の位相計算用）
	float ElapsedTime = 0.0f;

	//名前がBoneで始まるコンポーネントを頭→尾の順に配列へ格納する
	void CollectSpineBones();
};

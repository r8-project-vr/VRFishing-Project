// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AquariumBoidManager.generated.h"

class AAquariumBoidFish;
class UFishDataAsset;

//魚ごとの生成設定
USTRUCT(BlueprintType)
struct FAquariumFishSpawnSettings
{
	GENERATED_BODY()

public:
	//生成する魚データ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aquarium|Spawn")
	UFishDataAsset* FishData = nullptr;

	//この魚を生成する数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aquarium|Spawn", meta = (ClampMin = "1"))
	int32 SpawnCount = 10;
};

UCLASS()
class VRFISHING_API AAquariumBoidManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAquariumBoidManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//水槽の中心として使用するActor
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Aquarium|Bounds")
	TObjectPtr<AActor> SwimCenterActor;

	//魚が泳げる水槽の半径
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aquarium|Bounds", meta = (ClampMin = "100.0", Units = "cm"))
	float SwimRadius = 1500.0f;

	//水槽中心から水面・底までの距離
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aquarium|Bounds", meta = (ClampMin = "100.0", Units = "cm"))
	float SwimHalfHeight = 500.0f;

	//魚が壁を避け始める距離
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aquarium|Bounds", meta = (ClampMin = "0.0", Units = "cm"))
	float WallMargin = 200.0f;

	//水槽の泳動範囲をデバッグ表示するか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aquarium|Debug")
	bool bDrawDebugBounds = true;

	//生成に使用する魚Actorクラス
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aquarium|Spawn")
	TSubclassOf<AAquariumBoidFish> FishClass;

	//生成する魚と匹数の設定
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aquarium|Spawn")
	TArray<FAquariumFishSpawnSettings> FishSpawnSettings;

	//Managerが生成して管理している魚
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aquarium|Spawn")
	TArray<AAquariumBoidFish*> SpawnedFish;

	//設定された魚を水槽内に生成する
	UFUNCTION(BlueprintCallable, Category = "Aquarium|Spawn")
	void SpawnFish();

	//水槽中心の座標を取得する
	FVector GetSwimCenterLocation() const;

	//円柱範囲内のランダムな座標を取得する
	FVector GetRandomSpawnLocation() const;

	//Managerが管理している魚を移動させる
	void UpdateFishMovement(float DeltaTime);

	//円柱状の水槽境界から離れる力を計算する
	FVector CalculateBoundarySteering(const AAquariumBoidFish* FishActor) const;

};

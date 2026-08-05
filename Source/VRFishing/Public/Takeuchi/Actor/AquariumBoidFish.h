// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AquariumBoidFish.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UFishDataAsset;

UCLASS()
class VRFISHING_API AAquariumBoidFish : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAquariumBoidFish();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnConstruction(const FTransform& Transform) override;

public:	

	//魚のモデルを表示するコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AquariumFish|Mesh")
	USkeletalMeshComponent* FishMesh;

	//この魚に使用する魚データ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AquariumFish|Data")
	UFishDataAsset* FishData;

	//現在の移動速度と方向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AquariumFish|Movement")
	FVector Velocity = FVector::ZeroVector;

	//魚データをMeshへ反映する
	UFUNCTION(BlueprintCallable, Category = "AquariumFish|Data")
	void ApplyFishData();

	//魚Actorの基準となるRootComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AquariumFish|Components")
	USceneComponent* RootScene;
};

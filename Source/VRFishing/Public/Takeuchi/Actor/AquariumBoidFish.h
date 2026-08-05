// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AquariumBoidFish.generated.h"

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

public:	

	//魚のモデルを表示するコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AquariumFish|Mesh")
	USkeletalMeshComponent* FishMesh;

	//現在の移動速度と方向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AquariumFish|Movement")
	FVector Velocity = FVector::ZeroVector;

};

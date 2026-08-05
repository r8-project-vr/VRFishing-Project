// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AquariumBoidManager.generated.h"

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
};

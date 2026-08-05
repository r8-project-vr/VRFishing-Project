// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "DrawDebugHelpers.h"
#include "Takeuchi/Actor/AquariumBoidManager.h"

// Sets default values
AAquariumBoidManager::AAquariumBoidManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AAquariumBoidManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AAquariumBoidManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDrawDebugBounds)
	{
		return;
	}

	//Manager自身の位置を初期値にする
	FVector CenterLocation = GetActorLocation();

	//中心Actorが設定されている場合は、その位置を使用する
	if (SwimCenterActor)
	{
		CenterLocation = SwimCenterActor->GetActorLocation();
	}

	//円柱の上端と下端を計算する
	const FVector CylinderBottom =
		CenterLocation - FVector::UpVector * SwimHalfHeight;

	const FVector CylinderTop =
		CenterLocation + FVector::UpVector * SwimHalfHeight;

	//魚が泳げる円柱範囲を表示する
	DrawDebugCylinder(GetWorld(), CylinderBottom, CylinderTop, SwimRadius, 48, FColor::Cyan, false, 0.0f, 0, 3.0f);
}


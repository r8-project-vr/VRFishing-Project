// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Takeuchi/Actor/AquariumBoidFish.h"

// Sets default values
AAquariumBoidFish::AAquariumBoidFish()
{
	//魚ごとのTickは使用せず、Managerからまとめて更新する
	PrimaryActorTick.bCanEverTick = false;

	//魚モデルを表示するコンポーネントを作成する
	FishMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FishMesh"));
	RootComponent = FishMesh;

	//魚同士は物理衝突させない
	FishMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AAquariumBoidFish::BeginPlay()
{
	Super::BeginPlay();
	
}


// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.
#include "Takeuchi/Actor/AquariumBoidFish.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
AAquariumBoidFish::AAquariumBoidFish()
{
	//魚ごとのTickは使用せず、Managerからまとめて更新する
	PrimaryActorTick.bCanEverTick = false;

	//魚Actorの基準となるRootComponentを作成する
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	//魚モデルをRootSceneの子として作成する
	FishMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FishMesh"));
	FishMesh->SetupAttachment(RootScene);

	//魚同士は物理衝突させない
	FishMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AAquariumBoidFish::BeginPlay()
{
	Super::BeginPlay();

	ApplyFishData();
}

void AAquariumBoidFish::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyFishData();
}

void AAquariumBoidFish::ApplyFishData()
{
	if (!FishData)
	{
		return;
	}

	if (!FishMesh)
	{
		return;
	}

	//魚ごとのMeshを設定する
	FishMesh->SetSkeletalMesh(FishData->FishMesh);

	//AnimationBlueprintを設定する
	if (FishData->AnimationClass)
	{
		FishMesh->SetAnimInstanceClass(FishData->AnimationClass);
	}

	//魚ごとの表示サイズを設定する
	FishMesh->SetRelativeScale3D(FishData->MeshScale);

	//魚モデルの前方向を補正する
	FishMesh->SetRelativeRotation(FishData->MeshRotationOffset);
}


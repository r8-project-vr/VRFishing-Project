// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/Actor/BoidsFish.h"
#include "Tanimura/Actor/BoidsFishManager.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "EngineUtils.h"

ABoidsFish::ABoidsFish()
{
	// 移動はマネージャーがまとめて行うためTickは不要
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	// 魚種データのメッシュを表示するコンポーネント
	FishMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FishMesh"));
	FishMesh->SetupAttachment(RootScene);
	FishMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FishMesh->SetGenerateOverlapEvents(false);
}

void ABoidsFish::BeginPlay()
{
	Super::BeginPlay();

	// BPで設定済みの魚種データを見た目へ反映
	ApplyFishData();

	InitializeVelocity();

	// マネージャー未設定ならレベル内の最初のマネージャーへ登録
	if (!IsValid(Manager)) {
		for (TActorIterator<ABoidsFishManager> It(GetWorld()); It; ++It) {
			RegisterWithManager(*It);
			break;
		}
	}
}

void ABoidsFish::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromManager();

	Super::EndPlay(EndPlayReason);
}

FVector ABoidsFish::GetBoidsVelocity() const
{
	return Velocity;
}

float ABoidsFish::GetBoidsSpeed() const
{
	return Velocity.Size();
}

ABoidsFishManager* ABoidsFish::GetBoidsManager() const
{
	return Manager;
}

void ABoidsFish::SetBoidsVelocity(const FVector& InVelocity)
{
	Velocity = InVelocity;
	bVelocityInitialized = true;
}

void ABoidsFish::RegisterWithManager(ABoidsFishManager* InManager)
{
	if (!IsValid(InManager) || Manager == InManager) {
		return;
	}

	// 別のマネージャーへ登録済みなら先に解除
	if (IsValid(Manager)) {
		Manager->UnregisterAgent(this);
	}

	Manager = InManager;
	InitializeVelocity();
	InManager->RegisterAgent(this);
}

void ABoidsFish::UnregisterFromManager()
{
	if (IsValid(Manager)) {
		Manager->UnregisterAgent(this);
		Manager = nullptr;
	}
}

void ABoidsFish::InitializeVelocity()
{
	if (bVelocityInitialized) {
		return;
	}

	// ランダムまたはアクターの前方向へ初期速度を設定
	FVector Direction = bRandomizeInitialDirection ? FMath::VRand() : GetActorForwardVector();
	if (!Direction.Normalize()) {
		Direction = FVector::ForwardVector;
	}

	Velocity = Direction * FMath::Max(InitialSpeed, 0.0f);
	bVelocityInitialized = true;
}

void ABoidsFish::SetFishData(UFishDataAsset* InFishData)
{
	FishData = InFishData;

	// 解除時はメッシュも空にして見た目を消す
	if (!IsValid(FishData)) {
		if (FishMesh) {
			FishMesh->SetSkeletalMeshAsset(nullptr);
			FishMesh->SetAnimInstanceClass(nullptr);
		}
		return;
	}

	ApplyFishData();
}

void ABoidsFish::ApplyFishData()
{
	if (!FishMesh || !IsValid(FishData)) {
		return;
	}

	FishMesh->SetSkeletalMeshAsset(FishData->FishMesh);

	// アニメーションBPが設定されていれば適用
	if (FishData->AnimationClass) {
		FishMesh->SetAnimInstanceClass(FishData->AnimationClass);
	}

	FishMesh->SetRelativeScale3D(FishData->MeshScale);
	FishMesh->SetRelativeRotation(FishData->MeshRotationOffset);
}
